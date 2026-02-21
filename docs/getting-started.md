# Getting Started with Forge

This guide walks you through installing Forge, writing your first component, and running it in the browser.

---

## 1. Prerequisites

You need:
- **Clang** with `wasm32` target (to compile WASM)
- **make** (GNU Make or BSD Make)
- A modern web browser (Chrome, Firefox, Safari, Edge)

### Install Clang

**macOS:**
```bash
brew install llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"  # add to ~/.zshrc
```

**Ubuntu/Debian:**
```bash
apt install clang lld
```

**Verify:**
```bash
clang --target=wasm32-unknown-unknown --version
# Should print: clang version 16+ ...
```

---

## 2. Build Forge

```bash
git clone https://github.com/your-org/forge
cd forge
make
```

This produces:
```
build/
├── forge         ← the compiler CLI
└── forge-dev     ← the dev server
```

Optionally install globally:
```bash
sudo make install
# Installs to /usr/local/bin/forge and /usr/local/bin/forge-dev
```

---

## 3. Your First Component

Create a file `Greeting.cx`:

```c
#include <forge/web.h>

@component Greeting {

    @props {
        char *name;   /* who to greet */
    }

    @state {
        int count = 0;   /* click counter */
    }

    @style {
        font-family: system-ui, sans-serif;
        padding:     32px;
        text-align:  center;
    }

    @on(click) {
        state.count++;
    }

    @template {
        <div>
            <h1>Hello, {props.name}!</h1>
            <p>You clicked {state.count} times.</p>
            <button onclick={@click}>Click me</button>
        </div>
    }
}
```

---

## 4. Compile

```bash
forge compile -o dist Greeting.cx
```

Output:
```
dist/
├── Greeting.gen.c       ← generated C source
├── Greeting.wasm        ← compiled WebAssembly module
├── Greeting.forge.js    ← JavaScript wrapper
└── Greeting.forge.d.ts  ← TypeScript types
```

---

## 5. Use in HTML

Create `index.html`:

```html
<!DOCTYPE html>
<html>
<head>
  <script type="module" src="runtime/js/forge-runtime.js"></script>
  <script type="module" src="dist/Greeting.forge.js"></script>
</head>
<body>
  <forge-greeting name="World"></forge-greeting>
</body>
</html>
```

---

## 6. Run the Dev Server

```bash
forge-dev --dir ./ --port 3000
```

Open `http://localhost:3000` — the component renders instantly.

Edit `Greeting.cx`, save, and the browser hot-reloads automatically.

---

## 7. Next Steps

- Read [Component Syntax](component-syntax.md) for all directives
- Browse [examples/](../examples/) for real-world patterns
- Read [Runtime API](runtime-api.md) for advanced usage
- Read [Performance Guide](performance-guide.md) for optimization tips
