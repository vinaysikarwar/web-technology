/*
 * Forge Runtime - Core Runtime
 *
 * This file compiles to the forge_runtime.wasm module (or .a static lib)
 * that is linked into every component's WASM output.
 *
 * Responsibilities:
 *   - Memory and arena management
 *   - Context registry
 *   - Reactive update scheduler
 *   - Props serialization
 *   - Utility functions (sprintf, memset, etc.)
 */

#include "../include/forge/types.h"
#include "../include/forge/web.h"
#include "arena.h"
#include "registry.h"

#include <stdarg.h>

/* ─── External JS Imports ─────────────────────────────────────────────────── */

FORGE_IMPORT("env", "js_schedule_raf")
extern void js_schedule_raf(void);

FORGE_IMPORT("env", "js_console_log")
extern void js_console_log(u32 str_ptr, u32 str_len);

FORGE_IMPORT("env", "js_console_log_int")
extern void js_console_log_int(u32 label_ptr, u32 label_len, i64 val);

FORGE_IMPORT("env", "js_trap")
extern void js_trap(u32 msg_ptr, u32 msg_len) __attribute__((noreturn));

/* ─── Memory Utilities ────────────────────────────────────────────────────── */

void *forge_memset(void *dst, int c, size_t n) {
    u8 *p = (u8 *)dst;
    while (n--) *p++ = (u8)c;
    return dst;
}

void *forge_memcpy(void *dst, const void *src, size_t n) {
    u8       *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

int forge_memcmp(const void *a, const void *b, size_t n) {
    const u8 *ap = (const u8 *)a;
    const u8 *bp = (const u8 *)b;
    while (n--) {
        if (*ap != *bp) return *ap - *bp;
        ap++; bp++;
    }
    return 0;
}

int forge_strlen(const char *s) {
    int n = 0;
    while (*s++) n++;
    return n;
}

/* ─── Context Management ──────────────────────────────────────────────────── */

forge_ctx_t *forge_ctx_new(u32 el_id, u32 state_size, u32 props_size) {
    forge_ctx_t *ctx = FORGE_CALLOC(1, sizeof(forge_ctx_t));
    if (!ctx) return 0;
    ctx->el_id      = el_id;
    ctx->state_size = state_size;
    ctx->props_size = props_size;
    ctx->state      = FORGE_CALLOC(1, state_size);
    ctx->props      = FORGE_CALLOC(1, props_size);
    ctx->dirty      = 0;
    ctx->update_queued = 0;
    return ctx;
}

forge_ctx_t *forge_ctx_get(u32 el_id)        { return registry_get(el_id); }
void         forge_ctx_register(forge_ctx_t *ctx, u32 el_id) { registry_set(el_id, ctx); }
void         forge_ctx_unregister(u32 el_id) { registry_remove(el_id); }

void forge_ctx_free(forge_ctx_t *ctx) {
    /* Memory is arena-managed, no individual free needed.
     * Just zero out the context so the slot can be reused. */
    if (ctx) forge_memset(ctx, 0, sizeof(forge_ctx_t));
}

/* ─── Reactive Update Scheduler ───────────────────────────────────────────── */

static int _update_pending = 0;

void forge_schedule_update(forge_ctx_t *ctx) {
    ctx->update_queued = 1;
    if (!_update_pending) {
        _update_pending = 1;
        js_schedule_raf(); /* request animation frame from JS host */
    }
}

/* Called by the JS host on the RAF callback */
FORGE_EXPORT void forge_raf_callback(void) {
    _update_pending = 0;
    forge_flush_updates();
    arena_reset(&g_render_arena); /* free frame allocations */
}

void forge_flush_updates(void) {
    /* Walk all registered components, re-render dirty ones */
    /* Note: actual re-render calls are dispatched via the component's own
     * forge_update_<name> export, which the JS runtime knows about */
}

/* ─── Props Serialization ──────────────────────────────────────────────────── */

/*
 * Simple binary format (not JSON for speed):
 *   [field_count: u16]
 *   for each field:
 *     [field_index: u8] [kind: u8] [value: 8 bytes]
 *
 * For string fields:
 *     [kind=STRING] [ptr: u32] [len: u32]
 */

void forge_props_deserialize(void *props_dst, const u8 *blob, u32 len) {
    if (!blob || len < 2) return;
    /* For now: treat as a raw memory copy of the C struct.
     * The JS runtime serializes props in the exact layout of the struct
     * by using the type metadata emitted by the compiler. */
    forge_memcpy(props_dst, blob, len);
}

void forge_props_serialize(const void *props_src, u32 props_size, u8 **out, u32 *out_len) {
    *out     = FORGE_FRAME_ALLOC(props_size);
    *out_len = props_size;
    if (*out) forge_memcpy(*out, props_src, props_size);
}

/* ─── Event Routing ────────────────────────────────────────────────────────── */

int forge_event_is(const forge_event_t *e, const char *event_name) {
    return e->type_hash == forge_fnv1a(event_name);
}

/* ─── Tagged Values ────────────────────────────────────────────────────────── */

forge_val_t forge_val_int(i64 v)   { forge_val_t r; r.kind = FORGE_VAL_INT;   r.v.i = v;   return r; }
forge_val_t forge_val_float(f64 v) { forge_val_t r; r.kind = FORGE_VAL_FLOAT; r.v.f = v;   return r; }
forge_val_t forge_val_bool(int v)  { forge_val_t r; r.kind = FORGE_VAL_BOOL;  r.v.b = !!v; return r; }
forge_val_t forge_val_str(u32 p)   { forge_val_t r; r.kind = FORGE_VAL_STRING;r.v.str_ptr=p;return r;}
forge_val_t forge_val_null(void)   { forge_val_t r; r.kind = FORGE_VAL_NULL;  r.v.i = 0;   return r; }

/* ─── sprintf ──────────────────────────────────────────────────────────────── */

static char _sprintf_buf[4096];

u32 forge_sprintf(const char *fmt, ...) {
    /* Minimal sprintf implementation */
    char *out = _sprintf_buf;
    int   cap = sizeof(_sprintf_buf) - 1;
    int   pos = 0;
    va_list ap;
    va_start(ap, fmt);

    while (*fmt && pos < cap) {
        if (*fmt != '%') { out[pos++] = *fmt++; continue; }
        fmt++;
        switch (*fmt++) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && pos < cap) out[pos++] = *s++;
            break;
        }
        case 'd': {
            i64 v = va_arg(ap, int);
            if (v < 0) { out[pos++] = '-'; v = -v; }
            char tmp[24]; int tlen = 0;
            do { tmp[tlen++] = '0' + (int)(v % 10); v /= 10; } while (v > 0);
            for (int k = tlen - 1; k >= 0 && pos < cap; k--) out[pos++] = tmp[k];
            break;
        }
        case 'f': {
            f64 v = va_arg(ap, double);
            int iv = (int)v;
            /* integer part */
            char tmp[24]; int tlen = 0;
            int neg = iv < 0; if (neg) { out[pos++] = '-'; iv = -iv; }
            do { tmp[tlen++] = '0' + iv % 10; iv /= 10; } while (iv > 0);
            for (int k = tlen - 1; k >= 0 && pos < cap; k--) out[pos++] = tmp[k];
            /* decimal */
            out[pos++] = '.';
            int dec = (int)((v - (int)v) * 1000000);
            if (dec < 0) dec = -dec;
            char dt[8]; int dl = 0;
            for (int k = 0; k < 6; k++) { dt[dl++] = '0' + dec % 10; dec /= 10; }
            for (int k = dl - 1; k >= 0 && pos < cap; k--) out[pos++] = dt[k];
            break;
        }
        case 'c': out[pos++] = (char)va_arg(ap, int); break;
        case '%': out[pos++] = '%'; break;
        default:  out[pos++] = '?'; break;
        }
    }
    va_end(ap);
    out[pos] = '\0';
    /* Return pointer as WASM linear memory offset */
    return (u32)(uintptr_t)out;
}

/* ─── Logging ──────────────────────────────────────────────────────────────── */

void forge_log(const char *msg) {
    int len = forge_strlen(msg);
    js_console_log((u32)(uintptr_t)msg, (u32)len);
}

void forge_log_int(const char *label, i64 val) {
    int len = forge_strlen(label);
    js_console_log_int((u32)(uintptr_t)label, (u32)len, val);
}

void forge_trap(const char *msg) {
    int len = forge_strlen(msg);
    js_trap((u32)(uintptr_t)msg, (u32)len);
    /* unreachable — WASM trap */
    __builtin_unreachable();
}

/* ─── Runtime Initialization ──────────────────────────────────────────────── */

FORGE_EXPORT void forge_runtime_init(void) {
    forge_arena_init_all();
    registry_init();
}

/* Called once when WASM module starts (wasm start section) */
__attribute__((constructor))
static void _auto_init(void) {
    forge_runtime_init();
}
