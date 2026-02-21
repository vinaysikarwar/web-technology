# Runtime API Reference

Functions available inside component code via `#include <forge/web.h>`.

---

## Context

```c
forge_ctx_t *forge_ctx_get(u32 el_id);
```
Returns the component context for a given DOM element ID.

```c
void forge_schedule_update(forge_ctx_t *ctx);
```
Queue a re-render for this component. Called automatically after `@on` handlers — you rarely need to call this manually.

---

## DOM API

```c
forge_dom_node_t *forge_dom_create(forge_dom_node_t *parent, const char *tag);
```
Create an element and append to parent. Returns a node handle.

```c
void forge_dom_text(forge_dom_node_t *parent, const char *text);
```
Append a text node.

```c
void forge_dom_set_attr(forge_dom_node_t *el, const char *name, const char *value);
```
Set a DOM attribute.

```c
void forge_dom_on(forge_dom_node_t *el, const char *event, forge_event_cb cb, forge_ctx_t *ctx);
```
Attach an event listener.

```c
void forge_dom_remove(forge_dom_node_t *el);
void forge_dom_clear(forge_dom_node_t *parent);
```
Remove nodes.

---

## Memory

```c
// Allocate from persistent arena (survives across renders)
void *FORGE_ALLOC(size_t n)
void *FORGE_CALLOC(size_t count, size_t size)

// Allocate from render arena (reset each frame — for temporary strings)
void *FORGE_FRAME_ALLOC(size_t n)
```

---

## Strings

```c
u32 forge_sprintf(const char *fmt, ...);
```
Printf-style formatting. Allocates from render arena. Returns a WASM memory pointer. Valid only for the current frame.

Supported format specifiers: `%s`, `%d`, `%f`, `%c`, `%%`.

---

## Tagged Values

```c
forge_val_t forge_val_int(i64 v);
forge_val_t forge_val_float(f64 v);
forge_val_t forge_val_bool(int v);
forge_val_t forge_val_str(u32 ptr);
forge_val_t forge_val_null(void);

// Auto-detect type (C11 _Generic)
forge_val_t forge_val_auto(expr);
```

---

## Events

```c
int forge_event_is(const forge_event_t *e, const char *event_name);
```
Returns 1 if the event matches the given name.

**Event fields:**
```c
typedef struct {
    u32   type_hash;   // FNV-1a hash of event name
    u32   target_id;   // DOM node that fired
    i32   int_data;    // key code, mouse button, etc.
    f32   x, y;        // pointer coordinates
    u32   flags;       // shift|ctrl|alt|meta bitmask
} forge_event_t;
```

---

## Logging

```c
void forge_log(const char *msg);
void forge_log_int(const char *label, i64 val);
```
Calls `console.log` via JS import. Available in all builds.

---

## Utilities

```c
void *forge_memset(void *dst, int c, size_t n);
void *forge_memcpy(void *dst, const void *src, size_t n);
int   forge_memcmp(const void *a, const void *b, size_t n);
int   forge_strlen(const char *s);
```
Standard memory/string operations (no libc dependency).

```c
void forge_trap(const char *msg);   // abort with message
```

---

## Macros

```c
forge_val_auto(x)          // wrap C value as forge_val_t
FORGE_ASSERT(cond)         // runtime assertion
FORGE_ARRAY_LEN(arr)       // number of elements in static array
FORGE_MIN(a, b)
FORGE_MAX(a, b)
FORGE_CLAMP(v, lo, hi)
FORGE_LIKELY(x)            // branch prediction hint
FORGE_UNLIKELY(x)
```
