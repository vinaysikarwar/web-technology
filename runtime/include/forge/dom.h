/*
 * Forge Framework - DOM API
 *
 * All DOM operations are WASM imports — they call into the JS host.
 * The JS host maintains a compact table of DOM nodes keyed by integer ID,
 * enabling zero-overhead communication between WASM and the browser DOM.
 */

#ifndef FORGE_DOM_H
#define FORGE_DOM_H

#include "types.h"

/* ─── Node Creation ────────────────────────────────────────────────────────── */

/* Create a new element and append to parent. Returns node ID. */
FORGE_IMPORT("env", "forge_dom_create")
forge_dom_node_t *forge_dom_create(forge_dom_node_t *parent, const char *tag);

/* Create a text node */
FORGE_IMPORT("env", "forge_dom_text")
void forge_dom_text(forge_dom_node_t *parent, const char *text);

/* Create a dynamic expression node (re-evaluates on state change) */
FORGE_IMPORT("env", "forge_dom_expr")
void forge_dom_expr(forge_dom_node_t *parent, forge_expr_fn fn, void *ctx);

/* Look up an existing host-side DOM element by ID */
FORGE_IMPORT("env", "forge_dom_get")
forge_dom_node_t *forge_dom_get(u32 el_id);

/* Mount a nested Forge component */
FORGE_IMPORT("env", "forge_dom_create_component")
forge_dom_node_t *forge_dom_create_component(forge_dom_node_t *parent, const char *comp_name);

/* ─── Attribute Manipulation ───────────────────────────────────────────────── */

FORGE_IMPORT("env", "forge_dom_set_attr")
void forge_dom_set_attr(forge_dom_node_t *el, const char *name, const char *value);

FORGE_IMPORT("env", "forge_dom_set_attr_expr")
void forge_dom_set_attr_expr(forge_dom_node_t *el, const char *name,
                              forge_expr_fn fn, void *ctx);

FORGE_IMPORT("env", "forge_dom_set_prop")
void forge_dom_set_prop(forge_dom_node_t *el, const char *name, forge_val_t value);

FORGE_IMPORT("env", "forge_dom_set_prop_str")
void forge_dom_set_prop_str(forge_dom_node_t *el, const char *name, const char *value);

/* ─── Style ────────────────────────────────────────────────────────────────── */

FORGE_IMPORT("env", "forge_dom_set_style")
void forge_dom_set_style(forge_dom_node_t *el, const char *prop,
                          forge_expr_fn fn, const char *static_val);

/* Inject a CSS string into the document <head> (once per component type) */
FORGE_IMPORT("env", "forge_dom_inject_css")
void forge_dom_inject_css(const char *component_name, const char *css);

/* ─── Event Handling ───────────────────────────────────────────────────────── */

typedef void (*forge_event_cb)(forge_event_t *event, forge_ctx_t *ctx);

FORGE_IMPORT("env", "forge_dom_on")
void forge_dom_on(forge_dom_node_t *el, const char *event_name,
                   forge_event_cb cb, forge_ctx_t *ctx);

FORGE_IMPORT("env", "forge_dom_off")
void forge_dom_off(forge_dom_node_t *el, const char *event_name);

/* ─── DOM Mutation ─────────────────────────────────────────────────────────── */

FORGE_IMPORT("env", "forge_dom_remove")
void forge_dom_remove(forge_dom_node_t *el);

FORGE_IMPORT("env", "forge_dom_clear")
void forge_dom_clear(forge_dom_node_t *parent);

FORGE_IMPORT("env", "forge_dom_insert_before")
void forge_dom_insert_before(forge_dom_node_t *parent,
                               forge_dom_node_t *new_node,
                               forge_dom_node_t *ref_node);

/* ─── Keyed List Diffing ───────────────────────────────────────────────────── */

/* Begin a keyed list update on parent (used for @for directive output) */
FORGE_IMPORT("env", "forge_dom_list_begin")
void forge_dom_list_begin(forge_dom_node_t *parent);

FORGE_IMPORT("env", "forge_dom_list_item")
forge_dom_node_t *forge_dom_list_item(forge_dom_node_t *parent,
                                       const char *key, const char *tag);

FORGE_IMPORT("env", "forge_dom_list_end")
void forge_dom_list_end(forge_dom_node_t *parent);

#endif /* FORGE_DOM_H */
