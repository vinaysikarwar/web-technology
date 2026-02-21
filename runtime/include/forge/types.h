/*
 * Forge Framework - Core Types
 * Shared type definitions used by both runtime and compiler-generated code.
 */

#ifndef FORGE_TYPES_H
#define FORGE_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* ─── WASM Export Macro ───────────────────────────────────────────────────── */

#ifdef __wasm__
#  define FORGE_EXPORT __attribute__((visibility("default")))
#  define FORGE_IMPORT(module, name) \
       __attribute__((import_module(module), import_name(name)))
#else
#  define FORGE_EXPORT
#  define FORGE_IMPORT(module, name)
#endif

/* ─── Primitive Aliases ───────────────────────────────────────────────────── */

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;

/* ─── Forge Value (tagged union, for dynamic prop passing) ────────────────── */

typedef enum {
    FORGE_VAL_NULL   = 0,
    FORGE_VAL_INT    = 1,
    FORGE_VAL_FLOAT  = 2,
    FORGE_VAL_BOOL   = 3,
    FORGE_VAL_STRING = 4,   /* pointer into WASM linear memory */
    FORGE_VAL_FN     = 5,   /* function table index            */
} ForgeValKind;

typedef struct {
    ForgeValKind kind;
    union {
        i64    i;
        f64    f;
        i32    b;
        u32    str_ptr; /* offset in WASM memory */
        u32    fn_idx;  /* indirect call table index */
    } v;
} forge_val_t;

/* ─── DOM Node Handle ─────────────────────────────────────────────────────── */

typedef u32 forge_node_id;   /* index into the host-side DOM node table */

typedef struct forge_dom_node {
    forge_node_id id;
} forge_dom_node_t;

/* ─── Event ───────────────────────────────────────────────────────────────── */

typedef struct {
    u32     type_hash;    /* FNV-1a hash of event name for fast dispatch */
    u32     target_id;    /* DOM node that fired the event               */
    i32     int_data;     /* e.g. key code, mouse button                 */
    f32     x, y;         /* pointer coordinates                         */
    u32     flags;        /* modifier keys: shift|ctrl|alt|meta          */
} forge_event_t;

/* ─── Expression Function Pointer ─────────────────────────────────────────── */

typedef forge_val_t (*forge_expr_fn)(void *ctx);

/* ─── Memory Functions (provided by runtime, avoid libc dep) ─────────────── */

void *forge_memset(void *dst, int c, size_t n);
void *forge_memcpy(void *dst, const void *src, size_t n);
int   forge_memcmp(const void *a, const void *b, size_t n);
int   forge_strlen(const char *s);

/* ─── Component Context ───────────────────────────────────────────────────── */

typedef struct forge_ctx {
    u32    el_id;         /* host-side DOM element ID         */
    void  *props;         /* pointer to Props struct           */
    void  *state;         /* pointer to State struct           */
    u32    props_size;    /* sizeof(Props)                     */
    u32    state_size;    /* sizeof(State)                     */
    u32    dirty;         /* bitmask: which state fields changed */
    u32    update_queued; /* 1 if a re-render is scheduled     */
} forge_ctx_t;

/* ─── FNV-1a Hash (compile-time friendly) ─────────────────────────────────── */

static inline u32 forge_fnv1a(const char *s) {
    u32 h = 2166136261u;
    while (*s) { h ^= (u8)*s++; h *= 16777619u; }
    return h;
}

#define FORGE_EVENT_HASH(name) forge_fnv1a(name)

#endif /* FORGE_TYPES_H */
