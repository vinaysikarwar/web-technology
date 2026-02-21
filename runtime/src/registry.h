/*
 * Forge Runtime - Component Context Registry
 * Maps DOM element IDs to live component contexts.
 */

#ifndef FORGE_REGISTRY_H
#define FORGE_REGISTRY_H

#include "../include/forge/types.h"

#define FORGE_MAX_COMPONENTS 1024

/* ─── Registry ─────────────────────────────────────────────────────────────── */

void         registry_init(void);
forge_ctx_t *registry_get(u32 el_id);
void         registry_set(u32 el_id, forge_ctx_t *ctx);
void         registry_remove(u32 el_id);
int          registry_count(void);

/* Iterate all live contexts (for flush_updates) */
typedef void (*registry_each_fn)(forge_ctx_t *ctx, void *userdata);
void registry_each(registry_each_fn fn, void *userdata);

#endif /* FORGE_REGISTRY_H */
