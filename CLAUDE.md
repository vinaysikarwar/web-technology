# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

**Forge Framework** — a web component framework that compiles `.cx` (C-Extension) component files into WebAssembly or pure-JS bundles. Components are written in a custom C-like syntax and compile to Custom Elements (`<forge-button>`, `<forge-app>`, etc.).

## Build Commands

```bash
make                   # Build compiler + runtime + dev-server
make compiler          # Build only forge binary → build/forge
make runtime           # Build forge_runtime.a static library
make examples          # Compile all examples to their dist/ directories
make test              # Run lexer tests
make clean             # Remove all build artifacts
make install           # Install forge + forge-dev to /usr/local/bin
```

### Compiling a single example (no-wasm mode, with SSG pre-rendering)
```bash
./build/forge compile --no-wasm --prerender -o examples/04-property-site/dist \
    examples/04-property-site/PropertyCard.cx \
    examples/04-property-site/PropertyDetail.cx \
    examples/04-property-site/ContactForm.cx \
    examples/04-property-site/App.cx
```

### Starting the dev server
```bash
./build/forge-dev --dir examples/04-property-site --port 3000
```

### Key compiler flags
| Flag | Effect |
|------|--------|
| `--no-wasm` | Generate pure-JS DOM renderer (skip Clang/WASM) |
| `--prerender` | Emit SSG-pre-rendered static HTML (`--prerender` writes `*.forge.html`) |
| `-o <dir>` | Output directory (default: `./dist`) |
| `--ast` | Dump parsed AST to stdout and exit |
| `--no-types` | Skip TypeScript `.d.ts` generation |
| `--iife` | Emit IIFE JS instead of ES modules |

## Component Syntax (.cx Files)

```c
@component MyComponent {
    @props {
        char *label;         // immutable external data
        int   count;
        void (*onClick)(void); // callback prop
    }
    @state {
        int active = 0;      // mutable internal state (triggers re-render)
    }
    @style {
        background: {state.active ? "#4f46e5" : "#f8fafc"};  // reactive CSS
    }
    @computed {
        char *display = forge_sprintf("%s (%d)", props.label, props.count);
    }
    @on(click) {
        state.active = !state.active;
    }
    @template {
        <div class={state.active ? "active" : ""}>
            <h2>{computed.display}</h2>
            <button onclick={@click}>Toggle</button>
        </div>
    }
}
```

**Key rules:**
- State references in templates: `state.field`, `props.field`, `computed.field`
- Event handlers bound via `@on(event_name)` and referenced in templates as `onclick={@event_name}`
- Include child components: `#include "ChildComponent.cx"` at file top, then `<ChildComponent prop={...} />`
- Conditional rendering: `<if condition={...}>...</if>`
- List rendering: `<for each={state.list} as={item}>...</for>`

## Architecture

### Compilation Pipeline
```
.cx source → Lexer → Parser → AST → Analyzer → Codegen → C source (.gen.c)
                                                        ↓
                                              [if not --no-wasm] Clang → .wasm
                                                        ↓
                                              binding_gen → .forge.js + .d.ts
```

### Key compiler source files (`compiler/src/`)
- `main.c` — CLI entry, orchestrates the full pipeline, handles SSG pass
- `lexer.c/h` — tokenizes `.cx` files
- `parser.c/h` — recursive-descent parser, builds AST
- `ast.c/h` — all AST node type definitions (`ComponentNode`, `HtmlNode`, `TypeRef`, etc.)
- `analyzer.c/h` — semantic analysis; builds compile-time reactivity dependency graph by scanning `state.X` and `props.X` references
- `codegen.c/h` — generates C source from AST
- `binding_gen.c/h` — generates `.forge.js` Web Components wrapper; in `--no-wasm` mode, generates a full pure-JS DOM renderer from the AST

### No-WASM Mode
The `--no-wasm` flag (used by all current examples) skips Clang and instead generates a JavaScript DOM renderer directly from the AST in `binding_gen.c`. This is the current default for examples as it requires no WASM toolchain. Output is a `.forge.js` ES module that uses `customElements.define`.

### SSG / Pre-rendering
With `--prerender`, `binding_gen_prerender()` generates a static HTML snapshot used to populate the initial `<forge-app>` element contents in `index.html`. This gives search engines and view-source users meaningful HTML while JS hydrates in the background. Pre-rendered DOM nodes use `data-fid` attributes for hydration anchoring.

### Runtime (`runtime/`)
- `forge_runtime.c` — core DOM patching, event dispatch
- `arena.c` — arena allocator (no GC)
- `registry.c` — component context registry
- `js/forge-runtime.js` — browser bootstrap (~2KB)

### Stdlib (`stdlib/include/forge/`)
- `http.h` — `forge_http_get()` fetch bindings
- `router.h` — client-side hash router
- `store.h` — global reactive store across components
- `animate.h` — `forge_tween()` animation utilities

## Examples Structure

Each example in `examples/` follows this pattern:
1. `.cx` component files — source
2. `index.html` — entry with pre-rendered SSG HTML inside `<forge-app>`, plus `<script type="module">` blocks loading `dist/*.forge.js`
3. `dist/` — compiled output (`.forge.js` files, optional `.wasm`, `.d.ts`)

Example 04 (`examples/04-property-site/`) is the most complete: multi-component app with state-based routing, API data loading, filtering, detail view, and a contact form.

## Example 04 — Property Site Architecture

Components:
- `App.cx` — root component; owns `state.page` (0=home, 1=detail), `state.filter`, `state.selected_id`, `state.properties` (JSON array)
- `PropertyCard.cx` — listing card with computed formatted values
- `PropertyDetail.cx` — full detail view, receives all props from App
- `ContactForm.cx` — inquiry form with `state.submitted` toggle

Data flow: `api/properties.json` → fetched in `index.html` → injected into `app.properties` → component re-renders via `app._refresh()`

Routing: state-based (`state.page` switch) with URL hash updates for bookmarkability.
