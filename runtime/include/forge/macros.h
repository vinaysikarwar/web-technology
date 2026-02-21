/*
 * Forge Framework - Compiler Macros
 * Ergonomic helpers for component authors.
 */

#ifndef FORGE_MACROS_H
#define FORGE_MACROS_H

/* ─── forge_val_auto ──────────────────────────────────────────────────────── */
/* Automatically wrap a C expression as a forge_val_t based on type.
 * Uses C11 _Generic for zero-overhead type selection. */

#define forge_val_auto(x) _Generic((x),          \
    int:           forge_val_int((i64)(x)),        \
    long:          forge_val_int((i64)(x)),        \
    long long:     forge_val_int((i64)(x)),        \
    unsigned:      forge_val_int((i64)(x)),        \
    float:         forge_val_float((f64)(x)),      \
    double:        forge_val_float((f64)(x)),      \
    char *:        forge_val_str(forge_sprintf("%s",(x))), \
    const char *:  forge_val_str(forge_sprintf("%s",(x))), \
    default:       forge_val_null()                \
)

/* ─── Array helpers ───────────────────────────────────────────────────────── */

#define FORGE_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* ─── Min / Max ────────────────────────────────────────────────────────────── */

#define FORGE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define FORGE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define FORGE_CLAMP(v, lo, hi) FORGE_MIN(FORGE_MAX((v),(lo)),(hi))

/* ─── Stringify ────────────────────────────────────────────────────────────── */

#define FORGE_STR(x)    #x
#define FORGE_XSTR(x)   FORGE_STR(x)

/* ─── Unused parameter ─────────────────────────────────────────────────────── */

#define FORGE_UNUSED(x) ((void)(x))

/* ─── Static assert ────────────────────────────────────────────────────────── */

#define FORGE_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/* ─── Likely / Unlikely hints ──────────────────────────────────────────────── */

#ifdef __GNUC__
#  define FORGE_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define FORGE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define FORGE_LIKELY(x)   (x)
#  define FORGE_UNLIKELY(x) (x)
#endif

#endif /* FORGE_MACROS_H */
