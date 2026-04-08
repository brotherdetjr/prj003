#ifndef PEER_H
#define PEER_H

#include "../common/app.h"
#include "../../vendor/cjson/cJSON.h"

/* Set stdin to non-blocking. Call once at startup. */
void peer_stdin_init(void);

/*
 * Read any available lines from stdin and process them as peer messages.
 * Non-blocking — returns immediately if nothing is available.
 * Call on every iteration of the main loop.
 */
void peer_stdin_poll(app_t *app);

/* Send a peer message: write to stdout and push a peer_out SSE event. */
void peer_send(app_t *app, cJSON *msg);

#endif /* PEER_H */
