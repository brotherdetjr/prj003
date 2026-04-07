#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#include "../../core/world.h"
#include "emu.h"

/* ------------------------------------------------------------------ */
/* Emulator state                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    world_t world;
    int     auto_tick;
    char    filename[256]; /* active save file; empty = none */
    int     dirty;         /* unsaved changes */
} emu_state_t;

/* ------------------------------------------------------------------ */
/* Terminal                                                             */
/* ------------------------------------------------------------------ */

static struct termios s_orig_term;

static void term_raw(void)
{
    struct termios t = s_orig_term;
    t.c_lflag    &= ~(ICANON | ECHO);
    t.c_cc[VMIN]  = 0; /* non-blocking */
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

static void term_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_orig_term);
}

static void sig_handler(int sig)
{
    (void)sig;
    term_restore();
    exit(1);
}

/* Non-blocking: returns key code or -1 if nothing pending. */
static int try_read_key(void)
{
    unsigned char c;
    return read(STDIN_FILENO, &c, 1) == 1 ? (int)c : -1;
}

/* Blocking: waits for exactly one keypress (stays in raw mode). */
static int read_key(void)
{
    struct termios t = s_orig_term;
    t.c_lflag    &= ~(ICANON | ECHO);
    t.c_cc[VMIN]  = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    unsigned char c;
    read(STDIN_FILENO, &c, 1);
    term_raw();
    return (int)c;
}

/* ------------------------------------------------------------------ */
/* Formatting helpers                                                   */
/* ------------------------------------------------------------------ */

static void fmt_ts(char *buf, size_t n, uint64_t ts)
{
    time_t t = (time_t)ts;
    struct tm *tm = gmtime(&t);
    strftime(buf, n, "%Y-%m-%dT%H:%M:%S UTC", tm);
}

/* ------------------------------------------------------------------ */
/* Screen                                                               */
/* ------------------------------------------------------------------ */

static void screen_render(const emu_state_t *s)
{
    char ts_buf[32];

    printf("\033[2J\033[H");
    printf("=== TAMAGOTCHI EMULATOR ===\n\n");

    fmt_ts(ts_buf, sizeof(ts_buf), s->world.now_ts);
    printf("Virtual time  : %s\n", ts_buf);
    printf("File          : %s%s\n",
           s->filename[0] ? s->filename : "(none)",
           s->dirty       ? " [modified]" : "");
    printf("\n");

    if (!s->world.has_character) {
        printf("No character.  In console: 'new' to create one.\n");
    } else {
        const character_t *c = &s->world.character;
        char born_buf[32];
        fmt_ts(born_buf, sizeof(born_buf), c->birth_ts);
        printf("---- CHARACTER ----\n");
        printf("ID            : %08X\n",  c->id);
        printf("Born          : %s\n",    born_buf);
        printf("Energy        : %3u / 255\n", c->energy);
    }

    printf("\n");
    if (s->auto_tick)
        printf("[auto-tick]  ESC = console\n");
    else
        printf("[manual]  ENTER = tick   ESC = console\n");

    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* File I/O — minimal hand-written JSON, no library needed             */
/* ------------------------------------------------------------------ */

static int state_save(const world_t *w, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return -1; }

    fprintf(f, "{\n");
    fprintf(f, "  \"now_ts\": %llu,\n", (unsigned long long)w->now_ts);
    fprintf(f, "  \"has_character\": %s", w->has_character ? "true" : "false");

    if (w->has_character) {
        const character_t *c = &w->character;
        fprintf(f, ",\n");
        fprintf(f, "  \"character\": {\n");
        fprintf(f, "    \"id\": \"%08X\",\n",       c->id);
        fprintf(f, "    \"birth_ts\": %llu,\n",     (unsigned long long)c->birth_ts);
        fprintf(f, "    \"energy\": %u,\n",          c->energy);
        fprintf(f, "    \"drain_acc\": %u\n",        c->_drain_acc);
        fprintf(f, "  }\n");
    } else {
        fprintf(f, "\n");
    }

    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

/*
 * Minimal JSON reader for our fixed schema.
 * Finds the first occurrence of "key": and scans the value after it.
 */
static int json_read_ull(const char *json, const char *key, unsigned long long *out)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p == ' ') p++;
    return sscanf(p, "%llu", out) == 1 ? 0 : -1;
}

static int json_read_str(const char *json, const char *key, char *out, size_t n)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p == ' ') p++;
    if (*p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < n) out[i++] = *p++;
    out[i] = '\0';
    return 0;
}

static int json_read_bool(const char *json, const char *key, int *out)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p == ' ') p++;
    if (strncmp(p, "true",  4) == 0) { *out = 1; return 0; }
    if (strncmp(p, "false", 5) == 0) { *out = 0; return 0; }
    return -1;
}

static int state_load(world_t *w, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 4096) {
        fprintf(stderr, "%s: file too large or empty\n", path);
        fclose(f); return -1;
    }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);

    unsigned long long ull;
    int ok = 1;

    if (json_read_ull(buf, "now_ts", &ull) == 0)
        w->now_ts = (uint64_t)ull;
    else { ok = 0; }

    int has_char = 0;
    if (json_read_bool(buf, "has_character", &has_char) != 0) ok = 0;
    w->has_character = (uint8_t)has_char;

    if (ok && w->has_character) {
        char id_str[16];
        if (json_read_str(buf, "id", id_str, sizeof(id_str)) == 0)
            w->character.id = (uint32_t)strtoul(id_str, NULL, 16);
        else ok = 0;

        if (json_read_ull(buf, "birth_ts", &ull) == 0)
            w->character.birth_ts = (uint64_t)ull;
        else ok = 0;

        unsigned long long energy = 0, drain = 0;
        if (json_read_ull(buf, "energy",    &energy) == 0) w->character.energy     = (uint8_t)energy;
        else ok = 0;
        if (json_read_ull(buf, "drain_acc", &drain)  == 0) w->character._drain_acc = (uint16_t)drain;
        else ok = 0;
    }

    free(buf);

    if (!ok) {
        fprintf(stderr, "%s: malformed state file\n", path);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Console                                                              */
/* ------------------------------------------------------------------ */

/* Returns 1 = resume, 0 = stay in console, -1 = quit. */
static int console_handle(emu_state_t *s, const char *line)
{
    while (*line == ' ' || *line == '\t') line++;

    if (*line == '\0' || strcmp(line, "resume") == 0)
        return 1;

    if (strcmp(line, "tick") == 0) {
        world_tick(&s->world);
        s->dirty = 1;
        char ts[32];
        fmt_ts(ts, sizeof(ts), s->world.now_ts);
        printf("Ticked. Virtual time: %s\n", ts);
        return 0;
    }

    if (strcmp(line, "new") == 0) {
        if (s->world.has_character) {
            printf("Character already exists.\n");
        } else {
            uint32_t id = (uint32_t)rand();
            world_spawn_character(&s->world, id);
            s->dirty = 1;
            printf("Character created. ID: %08X\n", s->world.character.id);
        }
        return 0;
    }

    if (strncmp(line, "save", 4) == 0) {
        const char *arg = line + 4;
        while (*arg == ' ') arg++;
        if (*arg != '\0')
            snprintf(s->filename, sizeof(s->filename), "%s", arg);
        if (s->filename[0] == '\0') {
            printf("No filename specified. Usage: save [filename]\n");
            return 0;
        }
        if (state_save(&s->world, s->filename) == 0) {
            s->dirty = 0;
            printf("Saved to '%s'.\n", s->filename);
        }
        return 0;
    }

    if (strcmp(line, "exit") == 0) {
        if (s->dirty) {
            printf("Unsaved changes. Save before exit? [y/n/c] ");
            fflush(stdout);
            char ans[8];
            if (!fgets(ans, sizeof(ans), stdin)) return -1;
            if (ans[0] == 'c' || ans[0] == 'C') return 0;
            if (ans[0] == 'y' || ans[0] == 'Y') {
                if (s->filename[0] == '\0') {
                    printf("Filename: ");
                    fflush(stdout);
                    char fn[256];
                    if (fgets(fn, sizeof(fn), stdin)) {
                        fn[strcspn(fn, "\n")] = '\0';
                        snprintf(s->filename, sizeof(s->filename), "%s", fn);
                    }
                }
                if (s->filename[0]) state_save(&s->world, s->filename);
            }
        }
        return -1;
    }

    if (strcmp(line, "help") == 0) {
        printf(
            "Commands:\n"
            "  new              create a new character\n"
            "  tick             advance one tick\n"
            "  save [file]      save state to file\n"
            "  resume           return to main screen\n"
            "  exit             quit the emulator\n"
            "  help             show this help\n"
        );
        return 0;
    }

    printf("Unknown command '%s'. Type 'help' for commands.\n", line);
    return 0;
}

static void console_run(emu_state_t *s)
{
    term_restore();
    printf("\n--- CONSOLE ---  ('help' for commands, 'resume' to return)\n");
    char line[256];
    for (;;) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        int r = console_handle(s, line);
        if (r < 0) { term_restore(); exit(0); }
        if (r > 0) break;
    }
}

/* ------------------------------------------------------------------ */
/* Main loop                                                            */
/* ------------------------------------------------------------------ */

void emu_run(emu_config_t *cfg)
{
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    tcgetattr(STDIN_FILENO, &s_orig_term);
    srand((unsigned int)time(NULL));

    emu_state_t s = {0};
    s.auto_tick = cfg->auto_tick;

    if (cfg->filename[0]) {
        if (state_load(&s.world, cfg->filename) == 0) {
            snprintf(s.filename, sizeof(s.filename), "%s", cfg->filename);
        } else {
            fprintf(stderr, "Failed to load '%s' — starting fresh.\n", cfg->filename);
            uint64_t ts = cfg->start_ts ? cfg->start_ts : (uint64_t)time(NULL);
            world_init(&s.world, ts);
        }
    } else {
        uint64_t ts = cfg->start_ts ? cfg->start_ts : (uint64_t)time(NULL);
        world_init(&s.world, ts);
    }

    term_raw();

    for (;;) {
        screen_render(&s);

        if (s.auto_tick) {
            /* Poll for Esc every 100 ms; tick after a full second. */
            int remaining_ms = (int)(WORLD_TICK_S * 1000);
            int got_esc = 0;
            while (remaining_ms > 0) {
                usleep(100000);
                remaining_ms -= 100;
                if (try_read_key() == 27) { got_esc = 1; break; }
            }
            if (got_esc) {
                console_run(&s);
                term_raw();
            } else {
                world_tick(&s.world);
                s.dirty = 1;
            }
        } else {
            /* Manual: block until Enter/Space (tick) or Esc (console). */
            int k = read_key();
            if (k == 27) {
                console_run(&s);
                term_raw();
            } else if (k == '\r' || k == '\n' || k == ' ') {
                world_tick(&s.world);
                s.dirty = 1;
            }
        }
    }
}
