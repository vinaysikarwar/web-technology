/*
 * Forge Framework - WASM Emitter
 *
 * Drives the Clang/LLVM backend to compile generated .gen.c files
 * into WASM32 modules.
 *
 * Also provides a direct binary WASM emission path (no Clang needed)
 * for simple components — useful for fast incremental rebuilds.
 */

#ifndef FORGE_WASM_EMIT_H
#define FORGE_WASM_EMIT_H

#include <stdint.h>
#include <stddef.h>

/* ─── WASM Compilation Options ────────────────────────────────────────────── */

typedef struct {
    const char *clang_path;       /* path to clang, default "clang"     */
    const char *runtime_lib_dir;  /* forge runtime .a library path      */
    const char *include_dir;      /* forge headers directory             */
    int         optimize;         /* 0-3 optimization level              */
    int         debug;            /* emit DWARF debug info               */
    int         strip;            /* strip names from output             */
    int         async;            /* emit asyncify instrumentation       */
} WasmOptions;

/* ─── Compilation Result ──────────────────────────────────────────────────── */

typedef struct {
    int    success;
    char  *wasm_path;   /* output .wasm file path (heap-allocated)  */
    size_t wasm_size;   /* byte size of produced WASM module        */
    char  *error_msg;   /* compiler stderr output on failure        */
} WasmResult;

/* ─── Public API ──────────────────────────────────────────────────────────── */

/* Compile a .gen.c file to .wasm using Clang */
WasmResult wasm_compile(const char *c_source_path, const WasmOptions *opts);

/* Validate that clang + wasm32 target are available */
int wasm_check_toolchain(const WasmOptions *opts);

/* Get the wasm32 clang flags as a string (useful for debugging) */
char *wasm_build_flags(const WasmOptions *opts);

void wasm_result_free(WasmResult *r);

/* ─── WASM Binary Inspection ──────────────────────────────────────────────── */

/* Print the exports of a .wasm file (uses wasm-objdump if available) */
void wasm_print_exports(const char *wasm_path);

/* Return the size of a .wasm file in bytes */
size_t wasm_file_size(const char *wasm_path);

#endif /* FORGE_WASM_EMIT_H */
