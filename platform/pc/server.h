#ifndef SERVER_H
#define SERVER_H

#include "app.h"

/* Mongoose HTTP event handler — pass as fn to mg_http_listen. */
void mg_event_handler(struct mg_connection *c, int ev, void *ev_data);

/* mg_timer callback: advances one tick when autotick is enabled. */
void tick_timer_fn(void *arg);

/* Advance world by one tick and push the tick SSE event. */
void do_tick(app_t *app);

/* Push an SSE event to all subscribed clients. */
void sse_push(struct mg_mgr *mgr, const char *event, const char *data);

#endif /* SERVER_H */
