#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "server.h"
#include "state.h"
#include "lua_bind.h"
#include "../vendor/cjson/cJSON.h"

#define JSON_HDR "Content-Type: application/json\r\n"
#define IS_SSE(c) ((c)->data[0] == 'S')

/* ------------------------------------------------------------------ */
/* SSE                                                                */
/* ------------------------------------------------------------------ */

void sse_push(struct mg_mgr *mgr, const char *event, const char *data)
{
    for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) {
        if (!IS_SSE(c)) continue;
        mg_http_printf_chunk(c, "event: %s\ndata: %s\n\n", event, data);
    }
}

void lua_error_sse_cb(const char *fn, const char *msg, app_t *app)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "fn", fn);
    cJSON_AddStringToObject(data, "error", msg);
    char *s = cJSON_PrintUnformatted(data);
    sse_push(&app->mgr, "_on_lua_error", s);
    cJSON_free(s);
    cJSON_Delete(data);
}

/* ------------------------------------------------------------------ */
/* Autotick timer                                                     */
/* ------------------------------------------------------------------ */

void tick_timer_fn(void *arg)
{
    app_t *app = (app_t *)arg;
    if (!app->autotick) return;

    app->now_unix_sec = (uint64_t)time(NULL);

    uint64_t target = app->now_tick + AUTOTICK;
    while (app->now_tick < target) {
        uint64_t remaining = target - app->now_tick;
        advance_result_t r = app_advance(app, remaining, 1);
        if (!r.stopped_on_event) break;
        const char *evname = app->last_event_name[0]
                                 ? app->last_event_name
                                 : "unknown";
        fprintf(stderr, "event %s  now_tick=%llu\n",
                evname, (unsigned long long)r.now_tick);
        char data[64];
        snprintf(data, sizeof(data), "{\"now_tick\":%llu}",
                 (unsigned long long)r.now_tick);
        sse_push(&app->mgr, evname, data);
    }
}

/* ------------------------------------------------------------------ */
/* Response helpers                                                   */
/* ------------------------------------------------------------------ */

/*
 * Parse a cJSON item as a non-negative integer (no fractional part, >= 0).
 * Returns 1 and writes *out on success; returns 0 on failure.
 */
static int parse_uint(const cJSON *j, uint64_t *out)
{
    if (!cJSON_IsNumber(j)) return 0;
    double v = j->valuedouble;
    if (v < 0.0) return 0;
    uint64_t u = (uint64_t)v;
    if ((double)u != v) return 0;
    *out = u;
    return 1;
}

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
    cJSON *resp = app_state_to_json(app);
    cJSON_AddBoolToObject(resp, "ok", 1);
    char *s = cJSON_PrintUnformatted(resp);
    mg_http_reply(c, 200, JSON_HDR, "%s\n", s);
    cJSON_free(s);
    cJSON_Delete(resp);
}

/* ------------------------------------------------------------------ */
/* Command dispatch                                                   */
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

    if (strcmp(cmd, "advance_time") == 0) {
        cJSON *dur_j = cJSON_GetObjectItemCaseSensitive(body, "ticks");
        cJSON *stop_j = cJSON_GetObjectItemCaseSensitive(body, "stop_on_event");
        int stop_on_event = cJSON_IsTrue(stop_j);

        uint64_t ticks;
        if (!parse_uint(dur_j, &ticks)) {
            reply_error(c, "ticks must be a non-negative integer");
            goto done;
        }

        advance_result_t r = app_advance(app, ticks, stop_on_event);

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddNumberToObject(resp, "now_tick", (double)r.now_tick);
        cJSON_AddBoolToObject(resp, "stopped_on_event", r.stopped_on_event);
        if (r.stopped_on_event) {
            const char *evname = app->last_event_name[0]
                                     ? app->last_event_name
                                     : "unknown";
            fprintf(stderr, "event %s  now_tick=%llu\n",
                    evname, (unsigned long long)r.now_tick);
            cJSON_AddStringToObject(resp, "event", evname);
            char ev_data[64];
            snprintf(ev_data, sizeof(ev_data), "{\"now_tick\":%llu}",
                     (unsigned long long)r.now_tick);
            sse_push(&app->mgr, evname, ev_data);
        }
        char *resp_s = cJSON_PrintUnformatted(resp);
        mg_http_reply(c, 200, JSON_HDR, "%s\n", resp_s);
        cJSON_free(resp_s);
        cJSON_Delete(resp);

    } else if (strcmp(cmd, "get_state") == 0) {
        reply_state(c, app);

    } else if (strcmp(cmd, "get_autotick") == 0) {
        mg_http_reply(c, 200, JSON_HDR,
                      "{\"ok\":true,\"autotick\":%s}\n",
                      app->autotick ? "true" : "false");

    } else if (strcmp(cmd, "spawn") == 0) {
        if (app->has_character) {
            reply_error(c, "character already exists");
            goto done;
        }
        uint32_t char_id;
        cJSON *cid_j = cJSON_GetObjectItemCaseSensitive(body, "character_id");
        if (cJSON_IsString(cid_j))
            char_id = (uint32_t)strtoul(cid_j->valuestring, NULL, 16);
        else
            char_id = (uint32_t)rand();
        app_spawn_character(app, char_id);
        lua_bind_reset_rw(app);
        lua_bind_call(app, "on_spawn");
        reply_state(c, app);

    } else if (strcmp(cmd, "poof") == 0) {
        if (!app->has_character) {
            reply_error(c, "no character");
            goto done;
        }
        app_poof_character(app);
        reply_ok(c);

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

    } else if (strcmp(cmd, "set_wall_clock") == 0) {
        cJSON *wc_j = cJSON_GetObjectItemCaseSensitive(body, "now_unix_sec");
        uint64_t wc;
        if (!parse_uint(wc_j, &wc)) {
            reply_error(c, "now_unix_sec must be a non-negative integer");
            goto done;
        }
        app->now_unix_sec = wc;
        reply_ok(c);

    } else if (strcmp(cmd, "get_wall_clock") == 0) {
        mg_http_reply(c, 200, JSON_HDR,
                      "{\"ok\":true,\"now_unix_sec\":%llu}\n",
                      (unsigned long long)app->now_unix_sec);

    } else if (strcmp(cmd, "get_screen") == 0) {
        /* TODO: render actual PNG once graphics are implemented */
        reply_error(c, "not implemented");

    } else if (strcmp(cmd, "set_state") == 0) {
        cJSON *state_j = cJSON_GetObjectItemCaseSensitive(body, "state");
        if (!cJSON_IsObject(state_j)) {
            reply_error(c, "state must be an object");
            goto done;
        }
        if (json_to_state(app, state_j) != 0 ||
            lua_bind_restore(app, state_j) != 0) {
            reply_error(c, "invalid state");
            goto done;
        }
        reply_ok(c);

    } else {
        reply_error(c, "unknown command");
    }

done:
    cJSON_Delete(body);
}

/* ------------------------------------------------------------------ */
/* Mongoose event handler                                             */
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
                  "Transfer-Encoding: chunked\r\n"
                  "\r\n");
        c->data[0] = 'S'; /* mark as SSE subscriber */

    } else {
        mg_http_reply(c, 404, JSON_HDR,
                      "{\"ok\":false,\"error\":\"not found\"}\n");
    }
}
