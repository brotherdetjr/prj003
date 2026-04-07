#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "emu.h"

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "  --timeutc=YYYY-MM-DDTHH:MM:SS   set initial virtual time (UTC)\n"
        "  --noautotick                     require input to advance ticks\n"
        "  --file=FILENAME                  load state from file\n",
        prog);
}

static uint64_t parse_timeutc(const char *s)
{
    struct tm tm = {0};
    if (sscanf(s, "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        fprintf(stderr, "Invalid --timeutc format. Expected: YYYY-MM-DDTHH:MM:SS\n");
        exit(1);
    }
    tm.tm_year -= 1900;
    tm.tm_mon  -= 1;
    tm.tm_isdst = 0;
    time_t t = timegm(&tm);
    if (t == (time_t)-1) {
        fprintf(stderr, "Invalid --timeutc value.\n");
        exit(1);
    }
    return (uint64_t)t;
}

int main(int argc, char *argv[])
{
    emu_config_t cfg = {
        .start_ts  = 0,
        .auto_tick = 1,
        .filename  = {0}
    };

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--timeutc=", 10) == 0) {
            cfg.start_ts = parse_timeutc(argv[i] + 10);
        } else if (strcmp(argv[i], "--noautotick") == 0) {
            cfg.auto_tick = 0;
        } else if (strncmp(argv[i], "--file=", 7) == 0) {
            snprintf(cfg.filename, sizeof(cfg.filename), "%s", argv[i] + 7);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    emu_run(&cfg);
    return 0;
}
