#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../common/app.h"
#include "server.h"
#include "peer.h"
#include "state.h"
#include "lua_bind.h"

#define DEFAULT_PORT     "7070"

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "  --id=XXXXXXXX                             instance ID (8 hex digits); seeds PRNG\n"
        "  --port=N                                  HTTP port (default: " DEFAULT_PORT ")\n"
        "  --nowtick=N                               initial virtual clock in ms (now_tick)\n"
        "  --wallclockutc=YYYY-MM-DDTHH:MM:SS        initial wall-clock time (now_unix_sec)\n"
        "  --file=PATH                               load world state from JSON file\n"
        "  --script=PATH                             Lua game script (default: scripts/energy.lua\n"
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
    tm.tm_mon  -= 1;
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
    if (!f) { perror(path); return -1; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 65536) {
        fprintf(stderr, "%s: file too large or empty\n", path);
        fclose(f); return -1;
    }
    char *buf = malloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) { fprintf(stderr, "%s: malformed JSON\n", path); return -1; }

    int rc = json_to_world(&app->world, json);
    cJSON_Delete(json);
    if (rc != 0) { fprintf(stderr, "%s: invalid state\n", path); return -1; }
    return 0;
}

int main(int argc, char *argv[])
{
    app_t app = {0};
    app.autotick = 1;

    const char *port           = DEFAULT_PORT;
    uint64_t    arg_nowtick    = 0;   int has_nowtick    = 0;
    uint64_t    arg_wallclock  = 0;   int has_wallclock  = 0;
    char        load_file[256] = {0};
    char        script_arg[1024] = {0};
    uint32_t    given_id       = 0;
    int         has_id         = 0;

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

    world_init(&app.world, now_tick, now_unix_sec);

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
                     "%s/../../scripts/energy.lua", binary_path);
        } else {
            snprintf(script_path, sizeof(script_path), "scripts/energy.lua");
        }
    }

    if (lua_bind_init(&app, script_path) != 0) {
        fprintf(stderr, "Failed to initialise Lua from: %s\n", script_path);
        return 1;
    }

    if (load_file[0]) {
        if (load_state_file(&app, load_file) == 0) {
            /* explicit flags override values from the saved file */
            if (has_nowtick) {
                app.world.now_tick = arg_nowtick;
                world_rebuild_scheduler(&app.world);
            }
            if (has_wallclock)
                app.world.now_unix_sec = arg_wallclock;
            fprintf(stderr, "Loaded state from '%s'\n", load_file);
            if (app.world.has_character) {
                lua_bind_restore_scripted(&app);
                lua_bind_call0(&app, "on_restore");
            }
        } else {
            return 1;
        }
    }

    /* HTTP server */
    mg_mgr_init(&app.mgr);

    char addr[32];
    snprintf(addr, sizeof(addr), "http://0.0.0.0:%s", port);
    if (!mg_http_listen(&app.mgr, addr, mg_event_handler, &app)) {
        fprintf(stderr, "Failed to listen on %s\n", addr);
        return 1;
    }
    {
        time_t t = (time_t)app.world.now_unix_sec;
        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
        fprintf(stderr, "Gloxie %s  autotick %s  wall %s\n",
                app.instance_id, app.autotick ? "on" : "off", tbuf);
    }

    /* autotick timer */
    mg_timer_add(&app.mgr, AUTOTICK, MG_TIMER_REPEAT, tick_timer_fn, &app);

    /* peer stdin (non-blocking) */
    peer_stdin_init();

    /* main loop */
    for (;;) {
        mg_mgr_poll(&app.mgr, 100); /* 100 ms */
        peer_stdin_poll(&app);
    }
}
