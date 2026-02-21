/*
 * Forge Framework - Component Author API
 *
 * This is the ONLY header component authors need to include:
 *   #include <forge/web.h>
 *
 * It pulls in all necessary types, DOM operations, and runtime utilities.
 */

#ifndef FORGE_WEB_H
#define FORGE_WEB_H

#include "types.h"
#include "dom.h"
#include "macros.h"

/* ─── Context Registry ────────────────────────────────────────────────────── */

forge_ctx_t *forge_ctx_new(u32 el_id, u32 state_size, u32 props_size);
forge_ctx_t *forge_ctx_get(u32 el_id);
void         forge_ctx_free(forge_ctx_t *ctx);
void         forge_ctx_register(forge_ctx_t *ctx, u32 el_id);
void         forge_ctx_unregister(u32 el_id);

/* ─── Reactive Update Scheduler ───────────────────────────────────────────── */

/* Queue a re-render for this component on the next animation frame */
void forge_schedule_update(forge_ctx_t *ctx);

/* Immediately flush all pending re-renders (called by runtime after RAF) */
void forge_flush_updates(void);

/* ─── Props Serialization ──────────────────────────────────────────────────── */

/* Deserialize a JSON props blob into a C struct (binary format, not JSON text) */
void forge_props_deserialize(void *props_dst, const u8 *blob, u32 len);
void forge_props_serialize(const void *props_src, u32 props_size, u8 **out, u32 *out_len);

/* ─── Event Routing ────────────────────────────────────────────────────────── */

int forge_event_is(const forge_event_t *e, const char *event_name);

/* ─── Utilities ────────────────────────────────────────────────────────────── */

/* Tagged-value constructor helpers */
forge_val_t forge_val_int(i64 v);
forge_val_t forge_val_float(f64 v);
forge_val_t forge_val_bool(int v);
forge_val_t forge_val_str(u32 ptr);
forge_val_t forge_val_null(void);

/* Variadic-safe sprintf into WASM linear memory */
u32 forge_sprintf(const char *fmt, ...);

/* Assert (trap in WASM) */
#define FORGE_ASSERT(cond) do { if (!(cond)) forge_trap("assert failed: " #cond); } while(0)
void forge_trap(const char *msg) __attribute__((noreturn));

/* Console logging (calls JS console.log via import) */
void forge_log(const char *msg);
void forge_log_int(const char *label, i64 val);

#endif /* FORGE_WEB_H */
