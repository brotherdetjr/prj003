#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"
#include "state.h"
#include "../../vendor/cjson/cJSON.h"

#define JSON_HDR    "Content-Type: application/json\r\n"
#define IS_SSE(c)   ((c)->data[0] == 'S')

/* ------------------------------------------------------------------ */
/* SSE                                                                  */
/* ------------------------------------------------------------------ */

void sse_push(struct mg_mgr *mgr, const char *event, const char *data)
{
    for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) {
        if (!IS_SSE(c)) continue;
        mg_printf(c, "event: %s\ndata: %s\n\n", event, data);
    }
}

/* ------------------------------------------------------------------ */
/* Tick                                                                 */
/* ------------------------------------------------------------------ */

void do_tick(app_t *app)
{
    world_tick(&app->world);
    char data[64];
    snprintf(data, sizeof(data), "{\"now_ts\":%llu}",
             (unsigned long long)app->world.now_ts);
    sse_push(&app->mgr, "tick", data);
}

void tick_timer_fn(void *arg)
{
    app_t *app = (app_t *)arg;
    if (app->autotick)
        do_tick(app);
}

/* ------------------------------------------------------------------ */
/* Response helpers                                                     */
/* ------------------------------------------------------------------ */

static void reply_ok(struct mg_connection *c)
{
    mg_http_reply(c, 200, JSON_HDR, "{\"ok\":true}\n");
}

static void reply_error(struct mg_connection *c, const char *msg)
{
    mg_http_reply(c, 200, JSON_HDR,
                  "{\"ok\":false,\"error\":\"%s\"}\n", msg);
}

static void reply_state(struct mg_connection *c, app_t *app)
{
    cJSON *state = app_state_to_json(app);
    char  *s     = cJSON_PrintUnformatted(state);
    mg_http_reply(c, 200, JSON_HDR, "{\"ok\":true,\"state\":%s}\n", s);
    cJSON_free(s);
    cJSON_Delete(state);
}

/* ------------------------------------------------------------------ */
/* Command dispatch                                                     */
/* ------------------------------------------------------------------ */

static void handle_command(struct mg_connection *c,
                           struct mg_http_message *hm, app_t *app)
{
    cJSON *body = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!body) {
        mg_http_reply(c, 400, JSON_HDR,
                      "{\"ok\":false,\"error\":\"invalid JSON\"}\n");
        return;
    }

    cJSON *cmd_j = cJSON_GetObjectItemCaseSensitive(body, "cmd");
    if (!cJSON_IsString(cmd_j)) {
        reply_error(c, "missing cmd");
        goto done;
    }
    const char *cmd = cmd_j->valuestring;

    /* ---- tick ---- */
    if (strcmp(cmd, "tick") == 0) {
        do_tick(app);
        reply_ok(c);

    /* ---- get_state ---- */
    } else if (strcmp(cmd, "get_state") == 0) {
        reply_state(c, app);

    /* ---- spawn ---- */
    } else if (strcmp(cmd, "spawn") == 0) {
        if (app->world.has_character) {
            reply_error(c, "character already exists");
            goto done;
        }
        uint32_t char_id;
        cJSON *cid_j = cJSON_GetObjectItemCaseSensitive(body, "character_id");
        if (cJSON_IsString(cid_j))
            char_id = (uint32_t)strtoul(cid_j->valuestring, NULL, 16);
        else
            char_id = (uint32_t)rand();
        world_spawn_character(&app->world, char_id);
        reply_state(c, app);

    /* ---- escape ---- */
    } else if (strcmp(cmd, "escape") == 0) {
        if (!app->world.has_character) {
            reply_error(c, "no character");
            goto done;
        }
        app->world.has_character = 0;
        reply_ok(c);

    /* ---- set_autotick ---- */
    } else if (strcmp(cmd, "set_autotick") == 0) {
        cJSON *en = cJSON_GetObjectItemCaseSensitive(body, "enabled");
        if (!cJSON_IsBool(en)) {
            reply_error(c, "enabled must be a boolean");
            goto done;
        }
        app->autotick = cJSON_IsTrue(en) ? 1 : 0;
        mg_http_reply(c, 200, JSON_HDR,
                      "{\"ok\":true,\"autotick\":%s}\n",
                      app->autotick ? "true" : "false");

    /* ---- get_screen ---- */
    } else if (strcmp(cmd, "get_screen") == 0) {
        /* TODO: render actual PNG once graphics are implemented */
        reply_error(c, "not implemented");

    /* ---- set_state ---- */
    } else if (strcmp(cmd, "set_state") == 0) {
        cJSON *state_j = cJSON_GetObjectItemCaseSensitive(body, "state");
        if (!cJSON_IsObject(state_j)) {
            reply_error(c, "state must be an object");
            goto done;
        }
        world_t new_world;
        if (json_to_world(&new_world, state_j) != 0) {
            reply_error(c, "invalid state");
            goto done;
        }
        app->world = new_world;
        reply_ok(c);

    } else {
        reply_error(c, "unknown command");
    }

done:
    cJSON_Delete(body);
}

/* ------------------------------------------------------------------ */
/* Mongoose event handler                                               */
/* ------------------------------------------------------------------ */

void mg_event_handler(struct mg_connection *c, int ev, void *ev_data)
{
    app_t *app = (app_t *)c->fn_data;
    if (ev != MG_EV_HTTP_MSG) return;

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    if (mg_match(hm->uri, mg_str("/command"), NULL)) {
        if (mg_strcmp(hm->method, mg_str("POST")) != 0) {
            mg_http_reply(c, 405, JSON_HDR,
                          "{\"ok\":false,\"error\":\"POST required\"}\n");
            return;
        }
        handle_command(c, hm, app);

    } else if (mg_match(hm->uri, mg_str("/events"), NULL)) {
        mg_printf(c,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n\r\n");
        c->data[0] = 'S'; /* mark as SSE subscriber */

    } else {
        mg_http_reply(c, 404, JSON_HDR,
                      "{\"ok\":false,\"error\":\"not found\"}\n");
    }
}
