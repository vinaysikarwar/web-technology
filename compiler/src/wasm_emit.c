/*
 * Forge Framework - WASM Emitter Implementation
 *
 * Wraps the Clang compiler to produce wasm32 modules from generated C.
 */

#include "wasm_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ─── Toolchain Check ─────────────────────────────────────────────────────── */

int wasm_check_toolchain(const WasmOptions *opts) {
    const char *clang = opts && opts->clang_path ? opts->clang_path : "clang";
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "%s --target=wasm32-unknown-unknown --version > /dev/null 2>&1", clang);
    return system(cmd) == 0;
}

/* ─── Build Clang Flags ───────────────────────────────────────────────────── */

char *wasm_build_flags(const WasmOptions *opts) {
    const char *clang   = opts && opts->clang_path     ? opts->clang_path     : "clang";
    const char *inc_dir = opts && opts->include_dir    ? opts->include_dir    : "./runtime/include";
    const char *lib_dir = opts && opts->runtime_lib_dir? opts->runtime_lib_dir: "./runtime/build";
    int         optlvl  = opts ? opts->optimize : 2;
    int         debug   = opts ? opts->debug    : 0;
    int         strip   = opts ? opts->strip    : 0;

    char *flags = malloc(2048);
    int   pos   = 0;

    pos += snprintf(flags + pos, 2048 - pos,
        "%s"
        " --target=wasm32-unknown-unknown"
        " -nostdlib"
        " -O%d"
        " -I%s"
        " -L%s"
        " -lforge_runtime"
        " -Wl,--no-entry"
        " -Wl,--export-dynamic"
        " -Wl,--allow-undefined"
        " -Wl,-z,stack-size=65536"
        "%s%s",
        clang, optlvl, inc_dir, lib_dir,
        debug ? " -g" : "",
        strip ? " -Wl,--strip-all" : "");

    return flags;
}

/* ─── Compile ─────────────────────────────────────────────────────────────── */

WasmResult wasm_compile(const char *c_source_path, const WasmOptions *opts) {
    WasmResult result = { 0, NULL, 0, NULL };

    if (!c_source_path) {
        result.error_msg = strdup("No source file specified");
        return result;
    }

    /* Derive output path: Foo.gen.c → Foo.wasm */
    char out_path[512];
    strncpy(out_path, c_source_path, sizeof(out_path) - 1);
    char *dot = strrchr(out_path, '.');
    if (dot) strcpy(dot, ".wasm");
    else strcat(out_path, ".wasm");

    /* Build Clang flags */
    char *flags = wasm_build_flags(opts);

    /* Build full command */
    char cmd[4096];
    char err_file[] = "/tmp/forge_clang_err.txt";

    snprintf(cmd, sizeof(cmd),
             "%s %s -o %s 2>%s",
             flags, c_source_path, out_path, err_file);

    free(flags);

    /* Execute */
    int rc = system(cmd);
    if (rc != 0) {
        /* Read error output */
        FILE *ef = fopen(err_file, "r");
        if (ef) {
            fseek(ef, 0, SEEK_END);
            long fsz = ftell(ef);
            rewind(ef);
            result.error_msg = malloc((size_t)fsz + 1);
            if (result.error_msg) {
                fread(result.error_msg, 1, (size_t)fsz, ef);
                result.error_msg[fsz] = '\0';
            }
            fclose(ef);
        } else {
            result.error_msg = strdup("Compilation failed (no error output)");
        }
        return result;
    }

    /* Success */
    result.success   = 1;
    result.wasm_path = strdup(out_path);
    result.wasm_size = wasm_file_size(out_path);

    return result;
}

void wasm_result_free(WasmResult *r) {
    if (!r) return;
    free(r->wasm_path);
    free(r->error_msg);
    r->wasm_path = NULL;
    r->error_msg = NULL;
}

/* ─── WASM Binary Inspection ──────────────────────────────────────────────── */

void wasm_print_exports(const char *wasm_path) {
    char cmd[512];
    /* Try wasm-objdump first, fall back to wasm-dis */
    snprintf(cmd, sizeof(cmd),
             "wasm-objdump -x %s 2>/dev/null | grep -A100 'Export\\[' || "
             "echo '(wasm-objdump not installed — run: brew install wabt)'",
             wasm_path);
    system(cmd);
}

size_t wasm_file_size(const char *wasm_path) {
    struct stat st;
    if (stat(wasm_path, &st) == 0) return (size_t)st.st_size;
    return 0;
}
