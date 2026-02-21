# Component Syntax Reference

A complete reference for all Forge `.cx` component directives and template syntax.

---

## File Structure

```c
#include <forge/web.h>       // always include this
#include "OtherComponent.cx" // optional: import child components

@component ComponentName {
    @props    { ... }   // external inputs
    @state    { ... }   // internal state
    @style    { ... }   // scoped CSS
    @computed { ... }   // derived values
    @on(evt)  { ... }   // event handlers (one per event)
    @template { ... }   // HTML template
}
```

Sections can appear in any order. All sections except `@template` are optional.

---

## @props

Declares externally-passed data. Props are **read-only** inside the component.

```c
@props {
    // Primitive types
    int     count;
    float   ratio;
    char   *label;          // string (pointer to parent's memory)
    int     disabled;       // 0/1 used as boolean

    // Fixed-size string buffer
    char    name[64];

    // Callback props (function pointers)
    void (*onClick)(void);
    void (*onSelect)(int id, const char *value);

    // Custom struct props
    Color   background;     // user-defined type
}
```

**Passing props in template:**
```c
<MyButton
    label="Submit"
    count={state.items}
    onClick={@handle_submit}
/>
```

---

## @state

Declares internal mutable state. State changes trigger re-renders.

```c
@state {
    int    count    = 0;        // integer with initializer
    float  opacity  = 1.0f;
    int    loading  = 1;        // boolean flag
    char   input[256];          // char array (auto zero-initialized)
    char  *message  = "Hello";  // string pointer (be careful: must be stable)
}
```

**Accessing state:**
- In `@on` handlers: `state.count++`
- In `@computed`: `state.count * 2`
- In `@template`: `{state.count}`
- In `@style` values: `background: {state.active ? "blue" : "gray"};`

**State mutation rules:**
- Only mutate state inside `@on` handlers
- Mutations are batched — re-render happens once after handler returns
- Mutating state in `@computed` is undefined behavior

---

## @style

Scoped CSS applied to the component's root element. Static rules are compiled away to zero runtime cost. Dynamic rules re-evaluate when referenced state/props change.

```c
@style {
    /* Static rules — compiled to a <style> tag, zero runtime cost */
    display:         flex;
    flex-direction:  column;
    padding:         16px;
    border-radius:   8px;

    /* Dynamic rules — reactive, re-evaluate when deps change */
    background:      {state.active ? "#4f46e5" : "#ffffff"};
    color:           {props.dark   ? "#ffffff" : "#0f172a"};
    opacity:         {state.loading ? 0.5 : 1.0};
}
```

**Supported CSS properties:** All standard CSS properties are supported. Property names are passed through verbatim to the browser.

---

## @computed

Derived values that are memoized and recomputed only when their dependencies change.

```c
@computed {
    // Simple derived value
    int   total   = state.price * state.quantity;

    // String formatting (forge_sprintf returns WASM memory pointer)
    char *display = forge_sprintf("$%.2f", (float)computed.total / 100.0f);

    // Conditional
    int   is_over_budget = computed.total > props.budget;

    // Multi-step
    int   steps_left  = props.max_steps - state.step;
    float progress    = props.max_steps > 0
                        ? (float)state.step / (float)props.max_steps
                        : 0.0f;
}
```

**Rules:**
- Computed fields are evaluated in declaration order
- A computed field can reference earlier computed fields via `computed.name`
- Do not mutate state inside computed

---

## @on

Event handlers. One `@on` block per event name.

```c
@on(click) {
    state.count++;
    if (props.onClick) props.onClick();
}

@on(input) {
    // event is available as implicit parameter
    // event->int_data holds key codes for keyboard events
}

@on(submit) {
    if (state.input[0] == '\0') return;
    // process form...
}

@on(keydown) {
    if (event->int_data == 13) { /* Enter key */
        @submit();  // call another handler
    }
}
```

**Built-in event names:** `click`, `dblclick`, `mousedown`, `mouseup`, `mousemove`, `mouseenter`, `mouseleave`, `keydown`, `keyup`, `keypress`, `input`, `change`, `submit`, `focus`, `blur`, `scroll`, `resize`, `touchstart`, `touchend`, `touchmove`

**Custom events:** Any string is valid — `@on(my_custom_event)` will be dispatched when `forge_dispatch` is called with matching event name.

---

## @template

HTML template with embedded C expressions. Forge uses `{}` for expressions.

### Text Expressions

```c
<p>Count: {state.count}</p>
<p>Name: {props.label}</p>
<p>Total: {computed.total}</p>
<p>{state.loading ? "Loading..." : "Ready"}</p>
```

### Attribute Expressions

```c
<div class={state.active ? "active" : "inactive"}>
<input value={state.input}
       disabled={props.disabled ? "true" : "false"}>
<img src={props.url} alt={props.alt}>
```

### Event Binding

```c
<button onclick={@click}>Click</button>
<input oninput={state.text = event.value}>
<form onsubmit={@submit}>
```

### Nested Components

```c
<MyButton
    label="Click me"
    count={state.count}
    onClick={@handle_click}
/>

<UserCard
    user-id={state.selected_id}
    on-select={@user_selected}
/>
```

### Self-Closing Elements

```c
<img src="/logo.png" alt="Logo" />
<input type="text" value={state.val} />
<br />
<hr />
```

---

## Type System

Forge uses C types directly:

| C Type      | JS Equivalent | Notes                        |
|-------------|---------------|------------------------------|
| `int`       | `number`      | 32-bit signed integer        |
| `float`     | `number`      | 32-bit float                 |
| `double`    | `number`      | 64-bit float                 |
| `char*`     | `string`      | pointer, must stay valid     |
| `char[N]`   | `string`      | fixed buffer, zero-terminated|
| `int` (0/1) | `boolean`     | no native bool, use int      |
| `void(*)(…)`| `function`    | callback prop                |

---

## Preprocessor

Standard C preprocessor is supported:

```c
#include <forge/web.h>           // stdlib headers
#include <forge/http.h>          // HTTP client
#include "MyComponent.cx"        // import another component
#include "types.h"               // shared type definitions

#define MAX_ITEMS 100
#define BTN_COLOR "#4f46e5"
```

---

## Complete Example

```c
#include <forge/web.h>

@component SearchBox {

    @props {
        char  *placeholder;
        int    max_results;
        void (*onSearch)(const char *query);
    }

    @state {
        char input[256];
        int  focused   = 0;
        int  has_input = 0;
    }

    @style {
        position:    relative;
        display:     flex;
        align-items: center;
        border:      2px solid {state.focused ? "#4f46e5" : "#e2e8f0"};
        border-radius: 10px;
        padding:     8px 14px;
        background:  white;
        transition:  border-color 0.2s;
    }

    @computed {
        char *clear_label = state.has_input ? "✕" : "";
    }

    @on(input) {
        /* event->int_data would be key code for keydown;
           for input events the value is in input buffer */
        state.has_input = state.input[0] != '\0';
    }

    @on(search) {
        if (props.onSearch && state.input[0]) {
            props.onSearch(state.input);
        }
    }

    @on(clear) {
        state.input[0] = '\0';
        state.has_input = 0;
    }

    @on(focus) { state.focused = 1; }
    @on(blur)  { state.focused = 0; }

    @template {
        <div class="search-box">
            <span class="search-icon">🔍</span>
            <input
                type="text"
                placeholder={props.placeholder}
                value={state.input}
                oninput={@input}
                onfocus={@focus}
                onblur={@blur}
                onkeydown={if (event->int_data == 13) @search()}
            />
            <button
                class="clear-btn"
                onclick={@clear}
                style={state.has_input ? "" : "display:none"}>
                {computed.clear_label}
            </button>
        </div>
    }
}
```
