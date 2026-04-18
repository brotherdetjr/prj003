#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "peer.h"
#include "server.h"
#include "../../vendor/cjson/cJSON.h"

#define BUF_SIZE 4096

static char s_buf[BUF_SIZE];
static int  s_len = 0;

void peer_stdin_init(void)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

static void process_line(app_t *app, const char *line)
{
    if (!app->has_character) return; /* no character — ignore */

    cJSON *msg = cJSON_Parse(line);
    if (!msg) {
        fprintf(stderr, "peer: malformed message: %s\n", line);
        return;
    }

    /* TODO: apply game-logic effects (mood, compatibility, ...) */

    char *s = cJSON_PrintUnformatted(msg);
    sse_push(&app->mgr, "peer_in", s);
    cJSON_free(s);
    cJSON_Delete(msg);
}

void peer_stdin_poll(app_t *app)
{
    char tmp[512];
    ssize_t n;

    while ((n = read(STDIN_FILENO, tmp, sizeof(tmp))) > 0) {
        if (s_len + (int)n >= BUF_SIZE) {
            fprintf(stderr, "peer: input buffer overflow, discarding\n");
            s_len = 0;
        }
        memcpy(s_buf + s_len, tmp, (size_t)n);
        s_len += (int)n;
    }

    /* process all complete newline-terminated messages */
    char *start = s_buf;
    char *nl;
    while ((nl = memchr(start, '\n', (size_t)(s_buf + s_len - start))) != NULL) {
        *nl = '\0';
        if (nl > start) /* skip empty lines */
            process_line(app, start);
        start = nl + 1;
    }

    /* shift any incomplete tail to the front */
    int remaining = (int)(s_buf + s_len - start);
    if (remaining > 0 && start != s_buf)
        memmove(s_buf, start, (size_t)remaining);
    s_len = remaining;
}

void peer_send(app_t *app, cJSON *msg)
{
    if (!app->has_character) return;

    char *s = cJSON_PrintUnformatted(msg);
    puts(s);          /* stdout — orchestrator reads and routes */
    fflush(stdout);
    sse_push(&app->mgr, "peer_out", s);
    cJSON_free(s);
}
