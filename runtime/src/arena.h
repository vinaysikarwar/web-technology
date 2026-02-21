/*
 * Forge Runtime - Linear Memory Arena Allocator
 *
 * A bump-pointer allocator backed by WASM linear memory.
 * Allocations are O(1) — just add to pointer.
 * No per-object free — entire arena resets between renders.
 *
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │  WASM Linear Memory                                          │
 *  │  ┌────────────┬──────────────┬───────────────┬──────────┐   │
 *  │  │ stack/data │ Render Arena │ Persist Arena │  free →  │   │
 *  │  └────────────┴──────────────┴───────────────┴──────────┘   │
 *  └─────────────────────────────────────────────────────────────┘
 */

#ifndef FORGE_ARENA_H
#define FORGE_ARENA_H

#include <stddef.h>
#include <stdint.h>

#define FORGE_ARENA_RENDER_SIZE  (1 * 1024 * 1024)  /* 1 MB per frame  */
#define FORGE_ARENA_PERSIST_SIZE (4 * 1024 * 1024)  /* 4 MB persistent */
#define FORGE_ARENA_ALIGN        8                   /* 8-byte alignment */

typedef struct {
    uint8_t *base;
    uint8_t *ptr;
    uint8_t *end;
    size_t   peak;    /* high-water mark for diagnostics */
} Arena;

/* Initialize an arena backed by already-allocated memory */
void arena_init(Arena *a, void *base, size_t size);

/* Allocate `size` bytes from arena (returns NULL if out of space) */
void *arena_alloc(Arena *a, size_t size);

/* Allocate and zero-initialize */
void *arena_calloc(Arena *a, size_t count, size_t elem_size);

/* Reset: reclaim all memory (O(1), just reset pointer) */
void arena_reset(Arena *a);

/* Remaining bytes */
size_t arena_remaining(const Arena *a);

/* ── Global Arenas (initialized at runtime start) ── */

extern Arena g_render_arena;   /* reset each frame */
extern Arena g_persist_arena;  /* persists across frames */

/* Shorthand helpers */
#define FORGE_ALLOC(n)       arena_alloc(&g_persist_arena, (n))
#define FORGE_CALLOC(c, s)   arena_calloc(&g_persist_arena, (c), (s))
#define FORGE_FRAME_ALLOC(n) arena_alloc(&g_render_arena, (n))

#endif /* FORGE_ARENA_H */
