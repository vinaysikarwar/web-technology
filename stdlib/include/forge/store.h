/*
 * Forge Stdlib - Global Reactive Store
 *
 * A simple signal-based global state store. Components subscribe to
 * store slices and are automatically re-rendered when values change.
 *
 * Usage:
 *   #include <forge/store.h>
 *
 *   // Define your store shape
 *   typedef struct {
 *       int   count;
 *       char  username[64];
 *   } AppStore;
 *
 *   // Initialize
 *   forge_store_t *store = forge_store_create(sizeof(AppStore));
 *   AppStore *s = forge_store_get(store);
 *   s->count = 0;
 *
 *   // Update from anywhere (triggers re-render in subscribed components)
 *   forge_store_begin(store);
 *     forge_store_get(store)->count++;
 *   forge_store_commit(store);
 *
 *   // Subscribe from a component
 *   forge_store_subscribe(store, forge_ctx_get(el_id));
 */

#ifndef FORGE_STORE_H
#define FORGE_STORE_H

#include <forge/types.h>
#include <forge/web.h>

#define FORGE_STORE_MAX_SUBSCRIBERS 128

/* ─── Store Handle ─────────────────────────────────────────────────────────── */

typedef struct forge_store {
    void        *data;          /* heap pointer to user struct    */
    u32          data_size;
    forge_ctx_t *subscribers[FORGE_STORE_MAX_SUBSCRIBERS];
    int          sub_count;
    int          transaction;   /* 1 during begin/commit          */
} forge_store_t;

/* ─── Public API ───────────────────────────────────────────────────────────── */

forge_store_t *forge_store_create(u32 data_size);
void          *forge_store_get(forge_store_t *store);

/* Mutation: bracket store changes in begin/commit to batch updates */
void forge_store_begin(forge_store_t *store);
void forge_store_commit(forge_store_t *store);

/* Direct update (auto-commits) */
void forge_store_update(forge_store_t *store, void (*mutate)(void *data, void *userdata), void *userdata);

/* Subscription */
void forge_store_subscribe(forge_store_t *store, forge_ctx_t *ctx);
void forge_store_unsubscribe(forge_store_t *store, forge_ctx_t *ctx);

/* Derived value (memoized getter, only recomputes when store changes) */
typedef forge_val_t (*forge_selector_fn)(const void *store_data);
forge_val_t forge_store_select(forge_store_t *store, forge_selector_fn selector);

/* Destroy store */
void forge_store_free(forge_store_t *store);

/* ─── Singleton Store Helpers ─────────────────────────────────────────────── */

/* Register a named global store (accessible by name from any component) */
void           forge_store_register(const char *name, forge_store_t *store);
forge_store_t *forge_store_lookup(const char *name);

#endif /* FORGE_STORE_H */
