/*
 * Forge Framework - Code Generator
 *
 * Transforms the analyzed AST into C source code that, when compiled
 * with Clang targeting wasm32, produces a self-contained .wasm module.
 */

#ifndef FORGE_CODEGEN_H
#define FORGE_CODEGEN_H

#include "ast.h"
#include <stdio.h>

/* ─── Code Generator Options ──────────────────────────────────────────────── */

typedef struct {
    int minify;          /* strip whitespace from output  */
    int debug_info;      /* emit source-map comments      */
    int ssr_mode;        /* server-side rendering mode    */
} CodegenOptions;

/* ─── Public API ──────────────────────────────────────────────────────────── */

/* Generate C source for a single component, write to `out` stream.
 * Returns 0 on success, non-zero on error. */
int codegen_component(const ComponentNode *c, const CodegenOptions *opts, FILE *out);

/* Generate C source for an entire program */
int codegen_program(const Program *p, const CodegenOptions *opts, const char *out_dir);

#endif /* FORGE_CODEGEN_H */
