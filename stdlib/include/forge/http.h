/*
 * Forge Stdlib - HTTP (Fetch API Bindings)
 * Zero-copy async HTTP requests from WASM components.
 *
 * Usage:
 *   #include <forge/http.h>
 *
 *   forge_http_get("/api/users", my_callback, ctx);
 *
 *   void my_callback(forge_response_t *res, void *ctx) {
 *       if (res->ok) { ... use res->body, res->body_len ... }
 *   }
 */

#ifndef FORGE_HTTP_H
#define FORGE_HTTP_H

#include <forge/types.h>

/* ─── Response ─────────────────────────────────────────────────────────────── */

typedef struct {
    int     ok;         /* 1 if status 200-299    */
    int     status;     /* HTTP status code        */
    u8     *body;       /* response body (in arena)*/
    u32     body_len;
    char   *content_type;
    void   *userdata;
} forge_response_t;

typedef void (*forge_http_cb)(const forge_response_t *res, void *userdata);

/* ─── Request Options ──────────────────────────────────────────────────────── */

typedef struct {
    const char *method;   /* "GET", "POST", etc. default: "GET" */
    const char *body;     /* request body (for POST/PUT)         */
    u32         body_len;
    const char *headers;  /* "Key: Value\nKey2: Value2"          */
} forge_http_opts;

/* ─── Public API ───────────────────────────────────────────────────────────── */

void forge_http_get (const char *url, forge_http_cb cb, void *userdata);
void forge_http_post(const char *url, const char *body, u32 body_len,
                     forge_http_cb cb, void *userdata);
void forge_http_fetch(const char *url, const forge_http_opts *opts,
                      forge_http_cb cb, void *userdata);

/* ─── JSON Helpers ─────────────────────────────────────────────────────────── */

/* Minimal JSON reader — returns pointer to value for a key */
const char *forge_json_get(const char *json, const char *key);
int         forge_json_get_int(const char *json, const char *key, int default_val);
const char *forge_json_get_str(const char *json, const char *key);
int         forge_json_array_len(const char *json);
const char *forge_json_array_item(const char *json, int index);

#endif /* FORGE_HTTP_H */
