/*
 * Forge Framework - JS Binding Generator
 *
 * Generates a thin JavaScript loader/wrapper for each compiled WASM module.
 * The generated .js file:
 *   - Imports and instantiates the .wasm
 *   - Exposes a DOM custom element (<forge-button>) or a JS class
 *   - Routes browser events → WASM dispatch function
 *   - Handles prop serialization/deserialization
 *
 * In no-wasm + prerender mode, also generates:
 *   - Static HTML fragments for each component (.forge.html)
 *   - A fully assembled pre-rendered index.html
 *   - Hydration-aware JS that attaches to existing DOM
 */

#ifndef FORGE_BINDING_GEN_H
#define FORGE_BINDING_GEN_H

#include "ast.h"
#include <stdio.h>

typedef struct {
  int es_modules;    /* emit ESM (import/export) vs CommonJS/IIFE */
  int web_component; /* wrap as HTMLElement custom element        */
  int typescript;    /* emit .d.ts type declarations              */
  int no_wasm;       /* emit pure-JS DOM renderer (no WASM)       */
  int prerender;     /* emit pre-rendered static HTML + hydration  */
} BindingOptions;

int binding_gen_component(const ComponentNode *c, const BindingOptions *opts,
                          FILE *out);
int binding_gen_types(const ComponentNode *c, FILE *out); /* TypeScript .d.ts */

/* ─── Pre-rendering (SSG) ───────────────────────────────────────────────────
 * Generates static HTML for a component template, recursively inlining
 * child component content.
 *
 * `registry` is an array of all available ComponentNode pointers so that
 * when a <ChildComponent> is encountered, its template can be inlined.
 */
int binding_gen_prerender(const ComponentNode *c,
                          const ComponentNode **registry, int registry_count,
                          FILE *out);

/* ─── SSR Renderer (Node.js) ────────────────────────────────────────────────
 * Generates ComponentName.forge.ssr.js — a pure-JS Node.js module that
 * exports render(state, props) => HTML string.  No browser APIs used.
 */
int binding_gen_ssr_js(const ComponentNode *c,
                       const ComponentNode **registry, int registry_count,
                       FILE *out);

/* ─── SSR HTTP Server (Node.js) ─────────────────────────────────────────────
 * Generates forge-ssr-server.js — a ready-to-run Node.js SSR HTTP server.
 * Includes: API proxy, static file serving, template injection, clear-patch.
 * Users only need to fill in resolveState() with their API fetch logic.
 */
int binding_gen_ssr_server(const ComponentNode *c,
                            const ComponentNode **registry, int registry_count,
                            FILE *out);

#endif /* FORGE_BINDING_GEN_H */
