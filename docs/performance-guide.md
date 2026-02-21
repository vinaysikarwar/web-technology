# Forge Performance Guide

How to get maximum performance from Forge components.

---

## Baseline Performance

Before optimizing, understand where Forge already wins by default:

| Category              | What Forge Does                            | Benefit                    |
|-----------------------|--------------------------------------------|----------------------------|
| Startup               | AOT-compiled WASM, no JIT                  | Instant first execution    |
| DOM updates           | Direct DOM API, no VDOM diff               | O(1) per reactive field    |
| Memory                | Arena allocator, no GC                     | Zero GC pauses             |
| Bundle                | 2KB runtime + binary WASM                  | Fast network transfer      |
| Reactivity            | Compile-time dep graph, targeted updates   | No wasted work             |

---

## Optimization Levels

Compile with higher optimization for production:

```bash
# Development (fast compile, readable output)
forge compile -O0 -g src/App.cx

# Default (good balance)
forge compile -O2 src/App.cx

# Maximum (slower compile, best runtime perf)
forge compile -O3 --strip src/App.cx
```

The `-O3` flag passes `-O3 -flto` to Clang, enabling link-time optimization across the entire component.

---

## State Design

**Keep state minimal.** Every state field that is reactive triggers a DOM scan during re-render. Fields not used in `@template` are dead weight.

```c
// BAD: store derived data in state
@state {
    int  count        = 0;
    int  count_times2 = 0;  // derived! should be @computed
    char display[64];        // derived! should be @computed
}

// GOOD: only primitive state, derive the rest
@state {
    int count = 0;
}
@computed {
    int   count_times2 = state.count * 2;
    char *display      = forge_sprintf("%d", state.count);
}
```

---

## Batching State Updates

Multiple state mutations in a single `@on` handler are automatically batched — only one re-render fires after the handler returns.

```c
// GOOD: all mutations batched → single re-render
@on(update) {
    state.count++;
    state.loading = 0;
    state.name[0] = '\0';
}

// BAD: calling forge_schedule_update() multiple times (no-op but wasteful)
@on(update) {
    state.count++;
    forge_schedule_update(ctx);  // unnecessary, called automatically
}
```

---

## String Efficiency

`forge_sprintf` allocates from the **render arena** — reset each frame. Never store its result in `@state`.

```c
// BAD: storing render-arena pointer in persistent state
@state { char *label = ""; }
@on(click) {
    state.label = forge_sprintf("count: %d", state.count); // dangling after frame!
}

// GOOD: fixed-size buffer for persistent strings
@state { char label[64]; }
@on(click) {
    // forge_sprintf to temp, then copy if needed
    // or just use @computed:
}
@computed {
    char *label = forge_sprintf("count: %d", state.count);  // fresh each render
}
```

---

## List Rendering

For dynamic lists, avoid clearing and re-rendering the entire list on every update. Use the keyed list API:

```c
@on(refresh) {
    forge_dom_list_begin(list_parent);
    for (int i = 0; i < state.item_count; i++) {
        char key[32];
        forge_sprintf_to(key, "%d", state.items[i].id);
        forge_dom_node_t *item = forge_dom_list_item(list_parent, key, "li");
        // update item content
    }
    forge_dom_list_end(list_parent);
    // Forge automatically adds/removes/reorders DOM nodes
}
```

---

## Memory Sizing

Default arena sizes (in `arena.h`):
```c
#define FORGE_ARENA_RENDER_SIZE  (1 * 1024 * 1024)  // 1MB per frame
#define FORGE_ARENA_PERSIST_SIZE (4 * 1024 * 1024)  // 4MB total
```

For memory-constrained environments, reduce these:
```c
// In your build, override with:
#define FORGE_ARENA_RENDER_SIZE  (256 * 1024)  // 256KB
#define FORGE_ARENA_PERSIST_SIZE (1 * 1024 * 1024)  // 1MB
```

Check arena usage in development:
```c
forge_log_int("render peak KB", g_render_arena.peak / 1024);
forge_log_int("persist peak KB", g_persist_arena.peak / 1024);
```

---

## Component Splitting

Large components should be split into smaller ones. Each Forge component is a separate WASM module — loaded on-demand, cached by the browser.

```c
// Instead of one giant App component:
@component App {
    // 500 lines of template...
}

// Split into:
#include "Sidebar.cx"
#include "MainContent.cx"
#include "Header.cx"

@component App {
    @template {
        <div>
            <Header />
            <Sidebar />
            <MainContent />
        </div>
    }
}
// Each .cx compiles to a separate .wasm — loaded in parallel
```

---

## Avoiding Common Pitfalls

### Pitfall 1: Recomputing expensive values every render

```c
// BAD: expensive calculation in template expression
@template {
    <p>{compute_fibonacci(state.n)}</p>  // runs every re-render!
}

// GOOD: use @computed (memoized)
@computed {
    int fib_n = compute_fibonacci(state.n);  // only when state.n changes
}
@template {
    <p>{computed.fib_n}</p>
}
```

### Pitfall 2: Pointer to stack in props/state

```c
// BAD: pointer to stack-allocated string — undefined behavior
@on(click) {
    char tmp[64] = "hello";
    state.label = tmp;  // tmp goes out of scope!
}

// GOOD: fixed-size buffer
@state { char label[64]; }
@on(click) {
    // copy into buffer
    int i = 0;
    const char *src = "hello";
    while (*src && i < 63) state.label[i++] = *src++;
    state.label[i] = '\0';
}
```

### Pitfall 3: Triggering updates from computed fields

```c
// BAD: side effect in @computed
@computed {
    int doubled = state.count * 2;
    state.count = doubled; // infinite loop!
}
```

---

## Production Checklist

- [ ] Compile with `-O2` or `-O3`
- [ ] Use `--strip` to remove WASM symbol names
- [ ] Split large components into smaller modules
- [ ] Use `@computed` for all derived values
- [ ] Keep state fields minimal
- [ ] Set appropriate `Cache-Control` headers for `.wasm` files
- [ ] Enable gzip/brotli compression (WASM compresses 70%+)
- [ ] Use `forge_http_get` with caching for data fetching
- [ ] Monitor arena peak usage in staging

---

## WASM Compression

WASM binaries compress extremely well. Configure your server:

```nginx
# nginx
location ~* \.wasm$ {
    gzip_static on;
    add_header Content-Type application/wasm;
    add_header Cache-Control "public, max-age=31536000, immutable";
}
```

Typical compression ratios:
- 50KB `.wasm` → ~12KB gzip → ~9KB brotli
- Runtime `forge-runtime.js` → ~600 bytes gzip
