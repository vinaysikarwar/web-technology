/*
 * Forge Runtime - Component Context Registry
 */

#include "registry.h"
#include "arena.h"
#include <stddef.h>

/* ─── Hash Map (open addressing, linear probe) ────────────────────────────── */

typedef struct {
    u32          el_id;  /* 0 = empty slot */
    forge_ctx_t *ctx;
} RegistrySlot;

static RegistrySlot _slots[FORGE_MAX_COMPONENTS];
static int          _count = 0;

void registry_init(void) {
    for (int i = 0; i < FORGE_MAX_COMPONENTS; i++) {
        _slots[i].el_id = 0;
        _slots[i].ctx   = 0;
    }
    _count = 0;
}

static int slot_for(u32 el_id) {
    int idx = (int)(el_id % FORGE_MAX_COMPONENTS);
    for (int i = 0; i < FORGE_MAX_COMPONENTS; i++) {
        int s = (idx + i) % FORGE_MAX_COMPONENTS;
        if (_slots[s].el_id == 0 || _slots[s].el_id == el_id) return s;
    }
    return -1; /* full */
}

forge_ctx_t *registry_get(u32 el_id) {
    int s = slot_for(el_id);
    if (s < 0 || _slots[s].el_id != el_id) return 0;
    return _slots[s].ctx;
}

void registry_set(u32 el_id, forge_ctx_t *ctx) {
    int s = slot_for(el_id);
    if (s < 0) return; /* registry full */
    if (_slots[s].el_id == 0) _count++;
    _slots[s].el_id = el_id;
    _slots[s].ctx   = ctx;
}

void registry_remove(u32 el_id) {
    int s = slot_for(el_id);
    if (s < 0 || _slots[s].el_id != el_id) return;
    _slots[s].el_id = 0;
    _slots[s].ctx   = 0;
    _count--;
}

int registry_count(void) { return _count; }

void registry_each(registry_each_fn fn, void *userdata) {
    for (int i = 0; i < FORGE_MAX_COMPONENTS; i++) {
        if (_slots[i].el_id != 0 && _slots[i].ctx) {
            fn(_slots[i].ctx, userdata);
        }
    }
}
