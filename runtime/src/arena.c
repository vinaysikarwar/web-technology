/*
 * Forge Runtime - Arena Allocator Implementation
 */

#include "arena.h"
#include <stdint.h>

/* ─── Global Arenas ───────────────────────────────────────────────────────── */

Arena g_render_arena;
Arena g_persist_arena;

/* ─── Static backing memory ───────────────────────────────────────────────── */
/* In WASM this sits in linear memory; on host it's a static array */

static uint8_t _render_mem[FORGE_ARENA_RENDER_SIZE]  __attribute__((aligned(8)));
static uint8_t _persist_mem[FORGE_ARENA_PERSIST_SIZE] __attribute__((aligned(8)));

/* ─── Implementation ──────────────────────────────────────────────────────── */

void arena_init(Arena *a, void *base, size_t size) {
    a->base = (uint8_t *)base;
    a->ptr  = (uint8_t *)base;
    a->end  = (uint8_t *)base + size;
    a->peak = 0;
}

void *arena_alloc(Arena *a, size_t size) {
    /* Align up */
    uintptr_t p   = (uintptr_t)a->ptr;
    uintptr_t al  = (p + (FORGE_ARENA_ALIGN - 1)) & ~(uintptr_t)(FORGE_ARENA_ALIGN - 1);
    uint8_t  *new_ptr = (uint8_t *)al + size;

    if (new_ptr > a->end) return 0; /* out of memory */

    a->ptr = new_ptr;

    size_t used = (size_t)(a->ptr - a->base);
    if (used > a->peak) a->peak = used;

    return (void *)al;
}

void *arena_calloc(Arena *a, size_t count, size_t elem_size) {
    size_t total = count * elem_size;
    void  *p     = arena_alloc(a, total);
    if (!p) return 0;
    /* zero-fill */
    uint8_t *bp = (uint8_t *)p;
    for (size_t i = 0; i < total; i++) bp[i] = 0;
    return p;
}

void arena_reset(Arena *a) {
    a->ptr = a->base;
}

size_t arena_remaining(const Arena *a) {
    return (size_t)(a->end - a->ptr);
}

/* ─── Runtime Init (called once at WASM module start) ─────────────────────── */

void forge_arena_init_all(void) {
    arena_init(&g_render_arena,  _render_mem,  FORGE_ARENA_RENDER_SIZE);
    arena_init(&g_persist_arena, _persist_mem, FORGE_ARENA_PERSIST_SIZE);
}
