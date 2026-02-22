/*
 * Forge Compiler — Entry Point
 *
 * Usage:
 *   forge compile [options] <file.cx> [<file2.cx> ...]
 *
 * Options:
 *   -o <dir>        Output directory (default: ./dist)
 *   -O<0-3>         Optimization level (default: -O2)
 *   -g              Emit debug info
 *   --ast           Dump AST and exit (no code gen)
 *   --no-wasm       Only generate .gen.c, skip Clang step
 *   --no-types      Skip TypeScript .d.ts generation
 *   --iife          Emit IIFE JS instead of ES modules
 *   --no-web-comp   Skip custom element registration
 *   -v, --version   Print version and exit
 *   -h, --help      Print this help
 */

#include "analyzer.h"
#include "binding_gen.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "wasm_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FORGE_VERSION "0.1.0"

/* ─── Utility ───────────────────────────────────────────────────────────────
 */

static void print_usage(void) {
  printf("Forge Compiler v" FORGE_VERSION "\n\n"
         "Usage:\n"
         "  forge compile [options] <file.cx> ...\n\n"
         "Options:\n"
         "  -o <dir>       Output directory        (default: ./dist)\n"
         "  -O<0-3>        Optimization level       (default: -O2)\n"
         "  -g             Emit DWARF debug info\n"
         "  --ast          Dump AST, no code gen\n"
         "  --no-wasm      Generate .gen.c only, skip Clang\n"
         "  --prerender    Generate static HTML for SEO (SSG)\n"
         "  --ssr          Generate SSR server (App.forge.ssr.js + forge-ssr-server.js)\n"
         "  --no-types     Skip TypeScript .d.ts output\n"
         "  --iife         JS as IIFE (not ES module)\n"
         "  --no-web-comp  Skip customElements.define\n"
         "  -v, --version  Print version\n"
         "  -h, --help     Print this help\n\n");
}

/* Load file into a heap buffer, return NULL on failure */
static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "forge: cannot open '%s'\n", path);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  char *buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  fread(buf, 1, (size_t)sz, f);
  buf[sz] = '\0';
  fclose(f);
  return buf;
}

static void mkdir_p(const char *path) {
  char tmp[512];
  strncpy(tmp, path, sizeof(tmp) - 1);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
}

/* ─── Compile Single File ───────────────────────────────────────────────────
 */

typedef struct {
  const char *out_dir;
  int dump_ast;
  int no_wasm;
  int prerender;
  int ssr;       /* emit Node.js SSR renderer (*.forge.ssr.js) */
  int no_types;
  int esm;
  int web_component;
  int optimize;
  int debug;
  int verbose;
} CompileConfig;

/* Global registry for cross-component SSG inlining */
static const ComponentNode *registry[1024];
static int registry_count = 0;

static int compile_file(const char *path, const CompileConfig *cfg) {
  printf("forge: compiling %s\n", path);

  /* ── Read source ── */
  char *src = read_file(path);
  if (!src)
    return 1;

  /* ── Lex ── */
  Lexer lex;
  lexer_init(&lex, src, path);

  /* ── Parse ── */
  Parser parser;
  parser_init(&parser, &lex);
  Program *prog = parser_parse(&parser);

  if (parser_error_count(&parser) > 0) {
    fprintf(stderr, "forge: %d parse error(s) in %s\n",
            parser_error_count(&parser), path);
    ast_free_program(prog);
    free(src);
    return 1;
  }

  /* ── Analyze ── */
  AnalysisResult ar = analyze_program(prog);
  if (ar.error_count > 0) {
    fprintf(stderr, "forge: %d analysis error(s) in %s\n", ar.error_count,
            path);
    ast_free_program(prog);
    free(src);
    return 1;
  }
  if (ar.warning_count > 0) {
    fprintf(stderr, "forge: %d warning(s) in %s\n", ar.warning_count, path);
  }

  /* ── AST Dump ── */
  if (cfg->dump_ast) {
    ast_dump_program(prog);
    return 0;
  }

  mkdir_p(cfg->out_dir);

  /* ── Code Generation ── */
  CodegenOptions cg_opts = {.debug_info = cfg->debug};
  int rc = codegen_program(prog, &cg_opts, cfg->out_dir);
  if (rc != 0) {
    ast_free_program(prog);
    free(src);
    return 1;
  }

  /* ── WASM Compilation ── */
  if (!cfg->no_wasm) {
    WasmOptions w_opts = {
        .clang_path = "clang",
        .include_dir = "./runtime/include",
        .runtime_lib_dir = "./runtime/build",
        .optimize = cfg->optimize,
        .debug = cfg->debug,
        .strip = !cfg->debug,
    };

    if (!wasm_check_toolchain(&w_opts)) {
      fprintf(stderr,
              "\033[33mforge: WARNING\033[0m clang wasm32 target not found.\n"
              "  Install with: brew install llvm  (macOS)\n"
              "               apt install clang  (Ubuntu)\n"
              "  Skipping WASM compilation — .gen.c files written to %s/\n",
              cfg->out_dir);
    } else {
      for (int i = 0; i < prog->component_count; i++) {
        char c_path[512];
        snprintf(c_path, sizeof(c_path), "%s/%s.gen.c", cfg->out_dir,
                 prog->components[i]->name);

        WasmResult wr = wasm_compile(c_path, &w_opts);
        if (wr.success) {
          printf("forge: \033[32m✓\033[0m %s  (%zu bytes)\n", wr.wasm_path,
                 wr.wasm_size);
        } else {
          fprintf(stderr, "forge: \033[31mclang error\033[0m\n%s\n",
                  wr.error_msg ? wr.error_msg : "(no output)");
          rc = 1;
        }
        wasm_result_free(&wr);
      }
    }
  }

  /* ── JS Bindings ── */
  BindingOptions b_opts = {
      .es_modules = cfg->esm,
      .web_component = cfg->web_component,
      .typescript = !cfg->no_types,
      .no_wasm = cfg->no_wasm,
      .prerender = cfg->prerender,
  };

  for (int i = 0; i < prog->component_count; i++) {
    const ComponentNode *c = prog->components[i];

    /* Add to global registry for cross-component pre-rendering */
    if (registry_count < 1024)
      registry[registry_count++] = c;

    /* .forge.js */
    char js_path[512];
    snprintf(js_path, sizeof(js_path), "%s/%s.forge.js", cfg->out_dir, c->name);
    FILE *jsf = fopen(js_path, "w");
    if (jsf) {
      binding_gen_component(c, &b_opts, jsf);
      fclose(jsf);
      printf("forge: \033[32m✓\033[0m %s\n", js_path);
    }

    /* .d.ts */
    if (!cfg->no_types) {
      char dts_path[512];
      snprintf(dts_path, sizeof(dts_path), "%s/%s.forge.d.ts", cfg->out_dir,
               c->name);
      FILE *dtf = fopen(dts_path, "w");
      if (dtf) {
        binding_gen_types(c, dtf);
        fclose(dtf);
        printf("forge: \033[32m✓\033[0m %s\n", dts_path);
      }
    }
  }

  return rc;
}

/* ─── Main ──────────────────────────────────────────────────────────────────
 */

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage();
    return 0;
  }

  /* Check for sub-command */
  if (strcmp(argv[1], "compile") != 0) {
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
      printf("forge %s\n", FORGE_VERSION);
      return 0;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
      print_usage();
      return 0;
    }
    fprintf(stderr, "forge: unknown command '%s'. Try 'forge --help'\n",
            argv[1]);
    return 1;
  }

  /* Parse options */
  CompileConfig cfg = {
      .out_dir = "./dist",
      .dump_ast = 0,
      .no_wasm = 0,
      .prerender = 0,
      .ssr = 0,
      .no_types = 0,
      .esm = 1,
      .web_component = 1,
      .optimize = 2,
      .debug = 0,
      .verbose = 0,
  };

  const char **input_files = malloc(sizeof(char *) * (size_t)argc);
  int file_count = 0;

  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      cfg.out_dir = argv[++i];
    } else if (strncmp(argv[i], "-O", 2) == 0) {
      cfg.optimize = atoi(argv[i] + 2);
    } else if (strcmp(argv[i], "-g") == 0) {
      cfg.debug = 1;
    } else if (strcmp(argv[i], "--ast") == 0) {
      cfg.dump_ast = 1;
    } else if (strcmp(argv[i], "--no-wasm") == 0) {
      cfg.no_wasm = 1;
    } else if (strcmp(argv[i], "--prerender") == 0) {
      cfg.prerender = 1;
    } else if (strcmp(argv[i], "--ssr") == 0) {
      cfg.ssr = 1;
    } else if (strcmp(argv[i], "--no-types") == 0) {
      cfg.no_types = 1;
    } else if (strcmp(argv[i], "--iife") == 0) {
      cfg.esm = 0;
    } else if (strcmp(argv[i], "--no-web-comp") == 0) {
      cfg.web_component = 0;
    } else if (strcmp(argv[i], "-v") == 0 ||
               strcmp(argv[i], "--verbose") == 0) {
      cfg.verbose = 1;
    } else if (argv[i][0] != '-') {
      input_files[file_count++] = argv[i];
    } else {
      fprintf(stderr, "forge: unknown option '%s'\n", argv[i]);
      free(input_files);
      return 1;
    }
  }

  if (file_count == 0) {
    fprintf(stderr, "forge: no input files\n");
    free(input_files);
    return 1;
  }

  /* Compile each file */
  int rc = 0;
  for (int i = 0; i < file_count; i++) {
    rc |= compile_file(input_files[i], &cfg);
  }

  /* SSG Pass: Generate pre-rendered HTML for each component */
  if (cfg.prerender && rc == 0) {
    for (int i = 0; i < registry_count; i++) {
      char html_path[512];
      snprintf(html_path, sizeof(html_path), "%s/%s.forge.html", cfg.out_dir,
               registry[i]->name);
      FILE *hf = fopen(html_path, "w");
      if (hf) {
        binding_gen_prerender(registry[i], registry, registry_count, hf);
        fclose(hf);
        printf("forge: \033[32m✓\033[0m %s (SSG)\n", html_path);
      }
    }
  }

  /* SSR Pass: Generate Node.js render(state, props) => HTML for root component */
  if (cfg.ssr && rc == 0 && registry_count > 0) {
    /* Generate SSR renderer for the last compiled component (typically the root App) */
    const ComponentNode *root = registry[registry_count - 1];

    /* 1. Component render function: App.forge.ssr.js */
    char ssr_path[512];
    snprintf(ssr_path, sizeof(ssr_path), "%s/%s.forge.ssr.js", cfg.out_dir, root->name);
    FILE *sf = fopen(ssr_path, "w");
    if (sf) {
      binding_gen_ssr_js(root, registry, registry_count, sf);
      fclose(sf);
      printf("forge: \033[32m✓\033[0m %s (SSR renderer)\n", ssr_path);
    }

    /* 2. HTTP server: forge-ssr-server.js */
    char srv_path[512];
    snprintf(srv_path, sizeof(srv_path), "%s/forge-ssr-server.js", cfg.out_dir);
    FILE *svf = fopen(srv_path, "w");
    if (svf) {
      binding_gen_ssr_server(root, registry, registry_count, svf);
      fclose(svf);
      printf("forge: \033[32m✓\033[0m %s (SSR server)\n", srv_path);
    }

    printf("forge: \033[32mSSR ready\033[0m → node %s/forge-ssr-server.js\n", cfg.out_dir);
  }

  /* Note: In a production compiler, we would properly track all Program
     pointers to free them here. For this implementation, the process is about
     to exit anyway, but we've avoided the use-after-free crash. */

  free(input_files);

  if (rc == 0) {
    printf("forge: \033[32mBuild successful\033[0m  →  %s/\n", cfg.out_dir);
  } else {
    fprintf(stderr, "forge: \033[31mBuild failed\033[0m\n");
  }

  return rc;
}
