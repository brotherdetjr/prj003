#ifndef SERVER_H
#define SERVER_H

#include "app.h"

/* Load world state from a JSON file; calls json_to_state + lua_bind_restore.
 * Requires lua_bind_init to have been called first. Returns 0 on success. */
int load_state_file(app_t *app, const char *path);

/* Mongoose HTTP event handler — pass as fn to mg_http_listen. */
void mg_event_handler(struct mg_connection *c, int ev, void *ev_data);

/* Call _update then _draw at the current virtual tick. */
void update_and_draw(app_t *app);

/* mg_timer callback: advances one tick when autotick is enabled. */
void tick_timer_fn(void *arg);

/* Advance world by one tick and push the tick SSE event. */
void do_tick(app_t *app);

/* Push an SSE event to all subscribed clients. */
void sse_push(struct mg_mgr *mgr, const char *event, const char *data);

/* app_t.lua_error_cb implementation: emits a _on_lua_error SSE event. */
void lua_error_sse_cb(const char *fn, const char *msg, app_t *app);

#endif /* SERVER_H */
