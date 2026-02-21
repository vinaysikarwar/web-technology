/*
 * Forge Framework - JS Binding Generator
 *
 * Generates a thin JavaScript loader/wrapper for each compiled WASM module.
 * The generated .js file:
 *   - Imports and instantiates the .wasm
 *   - Exposes a DOM custom element (<forge-button>) or a JS class
 *   - Routes browser events → WASM dispatch function
 *   - Handles prop serialization/deserialization
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
} BindingOptions;

int binding_gen_component(const ComponentNode *c, const BindingOptions *opts,
                          FILE *out);
int binding_gen_types(const ComponentNode *c, FILE *out); /* TypeScript .d.ts */

#endif /* FORGE_BINDING_GEN_H */
