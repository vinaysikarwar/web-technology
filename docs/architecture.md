# Forge Architecture

A deep-dive into how Forge works from source to browser.

---

## System Overview

```
Developer writes .cx files
           │
           ▼
┌─────────────────────────────────────────────────────────────┐
│                     FORGE COMPILER                           │
│                                                              │
│  ┌────────┐  ┌────────┐  ┌──────────┐  ┌──────────┐        │
│  │ Lexer  │→ │ Parser │→ │ Analyzer │→ │ Codegen  │        │
│  └────────┘  └────────┘  └──────────┘  └──────────┘        │
│                                              │               │
│                                         ┌───┴───────────┐   │
│                                         │  .gen.c file  │   │
│                                         └───────────────┘   │
└─────────────────────────────────────────────────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────┐
│                     CLANG COMPILER                           │
│  --target=wasm32-unknown-unknown -nostdlib -O2               │
└─────────────────────────────────────────────────────────────┘
           │
           ▼
┌──────────────────────┐   ┌─────────────────────────────────┐
│   Component.wasm     │   │  Component.forge.js              │
│  (binary WASM module)│   │  (thin JS wrapper + loader)      │
└──────────────────────┘   └─────────────────────────────────┘
           │                          │
           └──────────────┬───────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                     BROWSER                                  │
│                                                              │
│  forge-runtime.js   ← 2KB bootstrap                         │
│  Component.forge.js ← loads Component.wasm                  │
│  Component.wasm     ← executes at near-native speed         │
│                                                              │
│  WebAssembly.Memory (linear memory)                          │
│  ┌──────────┬──────────────┬──────────────┬──────────────┐  │
│  │  stack   │ render arena │ persist arena│   free       │  │
│  └──────────┴──────────────┴──────────────┴──────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Compiler Pipeline

### Stage 1: Lexer (`lexer.c`)

Converts raw `.cx` text into a token stream.

**Mode switching:** The lexer maintains a mode flag that changes how bytes are interpreted:

- `LEX_MODE_C`: standard C token rules
- `LEX_MODE_TEMPLATE`: after `@template {`, switch to HTML tokenization
- `LEX_MODE_EXPR`: inside `{}` within a template, switch back to C
- `LEX_MODE_STYLE`: inside `@style { }`, tokenize as `property: value;`

This allows the lexer to correctly handle embedded C expressions in HTML:
```
<div class={state.active ? "on" : "off"}>
         ↑                              ↑
     LEX_MODE_EXPR (C mode)        back to HTML
```

### Stage 2: Parser (`parser.c`)

Recursive-descent parser that builds an AST. Key challenge: the template section mixes HTML structure and C expressions.

**Template parsing strategy:**
1. Parse HTML element tags
2. On `{`, switch to C expression mode — recursively parse until matching `}`
3. On `<ComponentName>`, detect by uppercase first letter → `HTML_COMPONENT` node
4. Attribute values can be string literals (`"..."`) or C expressions (`{...}`)

### Stage 3: Semantic Analyzer (`analyzer.c`)

**Reactivity graph construction:**

The analyzer walks every expression string in the AST and pattern-matches for:
- `state.fieldname` → marks `fieldname` as reactive in template
- `props.fieldname` → marks `fieldname` as used

This produces the **reactivity dependency graph** — a bitmask of which state fields affect which parts of the render output. The codegen uses this to generate targeted update functions instead of re-rendering everything on every state change.

**Example:**
```c
@state { int count = 0;  int color = 0; }
@template { <p>{state.count}</p> }
```
After analysis:
- `count.is_reactive = 1` (used in template)
- `color.is_reactive = 0` (unused — warning emitted)

### Stage 4: Code Generator (`codegen.c`)

Emits C source code with this structure per component:

```
ComponentName_Props  struct
ComponentName_State  struct
__compname_state_init()   initializes state to defaults
__compname_render()       builds DOM from template
__on_compname_event()     one function per @on handler
__computed_compname_x()   one function per @computed field
forge_mount_compname()    WASM export: called by JS on DOM mount
forge_update_compname()   WASM export: called when props change
forge_dispatch_compname() WASM export: routes events
forge_unmount_compname()  WASM export: cleanup
```

**Key design decision:** No virtual DOM. The codegen emits direct DOM API calls (`forge_dom_create`, `forge_dom_set_attr`, etc.) rather than building an intermediate tree to diff. Reactivity is handled by re-running only the specific update functions for changed state fields.

### Stage 5: Binding Generator (`binding_gen.c`)

Emits a `.forge.js` file that:
1. Asynchronously loads the `.wasm` module
2. Constructs the WASM import object (`env` namespace) with DOM bridge functions
3. Extends `ForgeComponent` (a custom element base class)
4. Registers a custom element (`<forge-button>`, etc.)
5. Routes `connectedCallback` / `attributeChangedCallback` → WASM exports

---

## Runtime Design

### Linear Memory Layout

```
WASM Linear Memory (64MB default)
┌─────────────┬───────────────┬────────────────┬──────────────────┐
│  Code/Data  │  Render Arena │  Persist Arena │   Future Growth  │
│  (static)   │  (1MB, reset  │  (4MB, keeps   │                  │
│             │   each frame) │   between       │                  │
│             │               │   renders)      │                  │
└─────────────┴───────────────┴────────────────┴──────────────────┘
   ~64KB         1MB              4MB
```

**Render Arena:** Allocated for temporary data needed during a single render pass (expression results, formatted strings). Reset to zero cost at the end of `forge_raf_callback`.

**Persist Arena:** Long-lived allocations: component contexts, state structs, props structs, event listener tables.

### DOM Bridge

The browser DOM is not directly accessible from WASM — only JS can touch it. Forge's runtime bridges this via a **DOM node ID table**:

```
WASM side:              JS side:
forge_dom_node_t        _nodeTable (Map<id → Node>)
{ id: 42 }     ←──────  document.createElement('div') → id=42
```

All DOM operations (`forge_dom_create`, `forge_dom_set_attr`, etc.) are declared as WASM imports and implemented in `forge-runtime.js`.

### Event Routing

```
User clicks button in browser
         ↓
Browser fires 'click' event
         ↓
forge-runtime.js handler fires
  - Builds forge_event_t struct in WASM linear memory
  - Calls indirect function table slot (the @on handler)
         ↓
WASM event handler runs (pure C code)
  - Mutates state
  - Calls forge_schedule_update(ctx)
         ↓
forge_schedule_update → js_schedule_raf (JS import)
         ↓
requestAnimationFrame callback fires
         ↓
forge_raf_callback() (WASM export called by JS)
  - Walks registry for dirty components
  - Re-runs reactive render functions
  - Resets render arena
```

### Reactivity Model

Forge uses **compile-time reactivity** — there is no reactive proxy or signal system at runtime. Instead:

1. The analyzer determines which state fields affect which DOM nodes
2. The codegen emits a specific `__update_fieldname()` function for each reactive field
3. When a state field is mutated, only its corresponding update function runs
4. This is O(1) per update, not O(n) tree traversal

Compare to React's VDOM: React re-renders the entire component tree on every state change, then diffs two VDOM trees. Forge runs exactly the minimal set of DOM mutations pre-computed at compile time.

---

## Memory Safety

Forge does not use a garbage collector. Instead:

- **Props**: pointed into parent's arena — valid for the lifetime of the parent
- **State**: allocated in persist arena — valid for the lifetime of the component
- **Render-time strings**: allocated in render arena — valid until next frame
- **Event handlers**: store function pointers (table indices), not closures

This means:
- No allocation overhead on hot paths
- No GC pauses during animations
- Deterministic, bounded memory usage

---

## Binary Component Format

A compiled Forge component is a standard WASM module. Its exports follow a naming convention:

```
forge_mount_<name>    (u32 el_id, u8* props_json, u32 len)
forge_update_<name>   (u32 el_id, u8* props_json, u32 len)
forge_dispatch_<name> (u32 el_id, forge_event_t* event)
forge_unmount_<name>  (u32 el_id)
forge_runtime_init    (void)
forge_raf_callback    (void)
memory                (WebAssembly.Memory)
```

This convention allows any JavaScript environment (not just forge-runtime.js) to host a Forge component — React wrappers, Vue plugins, etc.
