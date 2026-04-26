#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "../../common/app.h"
#include "../../common/server.h"
#include "peer.h"
#include "../../common/state.h"
#include "../../common/lua_bind.h"

static volatile sig_atomic_t s_stop = 0;
static void handle_stop(int sig)
{
    (void)sig;
    s_stop = 1;
}

#define DEFAULT_PORT "7070"
#define MAX_WATCHED_FILES 32

static char s_watched[MAX_WATCHED_FILES][1024];
static int s_n_watched;

static void refresh_watched_files(app_t *app, const char *script_path)
{
    strncpy(s_watched[0], script_path, 1023);
    s_watched[0][1023] = '\0';
    s_n_watched = 1 + lua_bind_get_loaded_files(app, s_watched + 1,
                                                MAX_WATCHED_FILES - 1);
}

static struct timespec watched_max_mtime(void)
{
    struct timespec latest = {0, 0};
    for (int i = 0; i < s_n_watched; i++) {
        struct stat st;
        if (stat(s_watched[i], &st) == 0) {
            struct timespec *mt = &st.st_mtim;
            if (mt->tv_sec > latest.tv_sec ||
                (mt->tv_sec == latest.tv_sec && mt->tv_nsec > latest.tv_nsec))
                latest = *mt;
        }
    }
    return latest;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [OPTIONS]\n"
            "  --id=XXXXXXXX                             instance ID (8 hex digits); seeds PRNG\n"
            "  --port=N                                  HTTP port (default: " DEFAULT_PORT ")\n"
            "  --nowtick=N                               initial virtual clock in ms (now_tick)\n"
            "  --wallclockutc=YYYY-MM-DDTHH:MM:SS        initial wall-clock time (now_unix_sec)\n"
            "  --file=PATH                               load world state from JSON file\n"
            "  --script=PATH                             Lua game script (default: scripts/main.lua\n"
            "                                            relative to this binary)\n"
            "  --noautotick                              start in manual-tick mode\n"
            "  --help                                    show this help and exit\n",
            prog);
}

static uint64_t parse_wallclockutc(const char *s)
{
    struct tm tm = {0};
    if (sscanf(s, "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        fprintf(stderr, "Invalid --wallclockutc format. "
                        "Expected: YYYY-MM-DDTHH:MM:SS\n");
        exit(1);
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = 0;
    time_t t = timegm(&tm);
    if (t == (time_t)-1) {
        fprintf(stderr, "Invalid --wallclockutc value\n");
        exit(1);
    }
    return (uint64_t)t;
}

static int load_state_file(app_t *app, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 65536) {
        fprintf(stderr, "%s: file too large or empty\n", path);
        fclose(f);
        return -1;
    }
    char *buf = malloc((size_t)sz + 1);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    buf[sz] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        fprintf(stderr, "%s: malformed JSON\n", path);
        return -1;
    }

    int rc = json_to_state(app, json);
    if (rc != 0) {
        cJSON_Delete(json);
        fprintf(stderr, "%s: invalid state\n", path);
        return -1;
    }

    lua_bind_restore(app, json);

    cJSON_Delete(json);
    return 0;
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, handle_stop);
    signal(SIGINT, handle_stop);

    app_t app = {0};
    app.autotick = 1;

    const char *port = DEFAULT_PORT;
    uint64_t arg_nowtick = 0;
    int has_nowtick = 0;
    uint64_t arg_wallclock = 0;
    int has_wallclock = 0;
    char load_file[256] = {0};
    char script_arg[1024] = {0};
    uint32_t given_id = 0;
    int has_id = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--id=", 5) == 0) {
            const char *s = argv[i] + 5;
            char *end;
            if (*s == '\0') {
                fprintf(stderr, "--id requires a non-empty hex value\n");
                return 1;
            }
            given_id = (uint32_t)strtoul(s, &end, 16);
            if (*end != '\0') {
                fprintf(stderr, "Invalid --id value: %s\n", s);
                return 1;
            }
            has_id = 1;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            port = argv[i] + 7;
        } else if (strncmp(argv[i], "--nowtick=", 10) == 0) {
            const char *s = argv[i] + 10;
            char *end;
            if (*s == '\0') {
                fprintf(stderr, "--nowtick requires a numeric value\n");
                return 1;
            }
            arg_nowtick = (uint64_t)strtoull(s, &end, 10);
            if (*end != '\0') {
                fprintf(stderr, "Invalid --nowtick value: %s\n", s);
                return 1;
            }
            has_nowtick = 1;
        } else if (strncmp(argv[i], "--wallclockutc=", 15) == 0) {
            arg_wallclock = parse_wallclockutc(argv[i] + 15);
            has_wallclock = 1;
        } else if (strncmp(argv[i], "--file=", 7) == 0) {
            if (*(argv[i] + 7) == '\0') {
                fprintf(stderr, "--file requires a non-empty path\n");
                return 1;
            }
            snprintf(load_file, sizeof(load_file), "%s", argv[i] + 7);
        } else if (strncmp(argv[i], "--script=", 9) == 0) {
            if (*(argv[i] + 9) == '\0') {
                fprintf(stderr, "--script requires a non-empty path\n");
                return 1;
            }
            snprintf(script_arg, sizeof(script_arg), "%s", argv[i] + 9);
        } else if (strcmp(argv[i], "--noautotick") == 0) {
            app.autotick = 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    /* instance ID + PRNG seed */
    app.instance_id_raw = has_id ? given_id : (uint32_t)time(NULL);
    snprintf(app.instance_id, sizeof(app.instance_id),
             "%08X", app.instance_id_raw);
    srand(app.instance_id_raw);

    /* wall clock: --wallclockutc if given, else real system time */
    uint64_t now_unix_sec = has_wallclock
                                ? arg_wallclock
                                : (uint64_t)time(NULL);

    /* virtual clock: --nowtick if given, else same as wall clock */
    uint64_t now_tick = has_nowtick ? arg_nowtick : now_unix_sec;

    app_init(&app, now_tick, now_unix_sec);

    /* Determine script path: --script overrides; otherwise relative to binary */
    char script_path[1024];
    if (script_arg[0] != '\0') {
        snprintf(script_path, sizeof(script_path), "%s", script_arg);
    } else {
        char binary_path[512];
        ssize_t n = readlink("/proc/self/exe", binary_path, sizeof(binary_path) - 1);
        if (n > 0) {
            binary_path[n] = '\0';
            char *slash = strrchr(binary_path, '/');
            if (slash) *slash = '\0';
            snprintf(script_path, sizeof(script_path),
                     "%s/../../scripts/main.lua", binary_path);
        } else {
            snprintf(script_path, sizeof(script_path), "scripts/main.lua");
        }
    }

    {
        char canon[4096];
        const char *resolved = realpath(script_path, canon);
        fprintf(stderr, "Script: %s\n", resolved ? resolved : script_path);
    }
    if (lua_bind_init(&app, script_path) != 0) {
        fprintf(stderr, "Failed to initialise Lua from: %s\n", script_path);
        return 1;
    }

    if (load_file[0]) {
        if (load_state_file(&app, load_file) == 0) {
            /* explicit flags override values from the saved file */
            if (has_nowtick) {
                app.now_tick = arg_nowtick;
                scheduler_clear(&app.scheduler);
            }
            if (has_wallclock)
                app.now_unix_sec = arg_wallclock;
            fprintf(stderr, "Loaded state from '%s'\n", load_file);
        } else {
            lua_close(app.L);
            return 1;
        }
    }

    /* HTTP server */
    mg_mgr_init(&app.mgr);

    char addr[32];
    snprintf(addr, sizeof(addr), "http://0.0.0.0:%s", port);
    if (!mg_http_listen(&app.mgr, addr, mg_event_handler, &app)) {
        fprintf(stderr, "Failed to listen on %s\n", addr);
        lua_close(app.L);
        mg_mgr_free(&app.mgr);
        return 1;
    }
    {
        time_t t = (time_t)app.now_unix_sec;
        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
        fprintf(stderr, "Gloxie %s  autotick %s  wall %s\n",
                app.instance_id, app.autotick ? "on" : "off", tbuf);
    }

    /* autotick timer */
    mg_timer_add(&app.mgr, AUTOTICK, MG_TIMER_REPEAT, tick_timer_fn, &app);

    /* peer stdin (non-blocking) */
    peer_stdin_init();

    /* seed watcher with the exact set of Lua files loaded at startup */
    refresh_watched_files(&app, script_path);
    struct timespec lua_mtime = watched_max_mtime();

    /* main loop */
    while (!s_stop) {
        mg_mgr_poll(&app.mgr, 100); /* 100 ms */
        peer_stdin_poll(&app);

        struct timespec cur = watched_max_mtime();
        if (cur.tv_sec != lua_mtime.tv_sec || cur.tv_nsec != lua_mtime.tv_nsec) {
            lua_mtime = cur;
            fprintf(stderr, "Hot-reloading: %s\n", script_path);
            if (lua_bind_reload(&app, script_path) == 0) {
                refresh_watched_files(&app, script_path);
                char reload_data[64];
                snprintf(reload_data, sizeof(reload_data),
                         "{\"now_tick\":%llu}",
                         (unsigned long long)app.now_tick);
                sse_push(&app.mgr, "_on_reload", reload_data);
                fprintf(stderr, "Script reloaded\n");
            } else {
                fprintf(stderr, "Reload failed, previous script still running\n");
            }
        }
    }

    lua_close(app.L);
    mg_mgr_free(&app.mgr);
    return 0;
}
