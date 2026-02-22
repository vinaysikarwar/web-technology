# Forge Framework — Complete Developer Guide

> Build multi-page web applications with `.cx` components, server-side rendering, real URL routing, and zero JavaScript framework dependencies.

---

## Table of Contents

1. [Installation & Setup](#1-installation--setup)
2. [Component Syntax](#2-component-syntax)
3. [Compiling Components](#3-compiling-components)
4. [Building an Application](#4-building-an-application)
5. [Routing (History API)](#5-routing-history-api)
6. [SSG — Static Site Generation](#6-ssg--static-site-generation)
7. [SSR — Server-Side Rendering](#7-ssr--server-side-rendering)
8. [SEO — Dynamic Meta Tags](#8-seo--dynamic-meta-tags)
9. [API Integration & Data Loading](#9-api-integration--data-loading)
10. [Component Communication](#10-component-communication)
11. [Cart & Local State](#11-cart--local-state)
12. [Deployment](#12-deployment)
13. [Complete Example Walkthrough](#13-complete-example-walkthrough)

---

## 1. Installation & Setup

### Build the compiler

```bash
git clone <repo>
cd WebTechnology
make compiler          # builds build/forge
make dev-server        # builds build/forge-dev (optional)
```

### No Clang/WASM toolchain required

All current examples use `--no-wasm` mode which generates pure JavaScript with **no Clang dependency**. Only `make compiler` is needed.

```bash
# Verify it works
./build/forge --version   # → forge 0.1.0
```

---

## 2. Component Syntax

Components are written in `.cx` files using a C-like syntax. Each file can contain one `@component` block.

### Full Component Anatomy

```c
#include "ChildComponent.cx"   /* import child components */

@component MyComponent {

    /* ── Props: immutable input from parent ────────────────────────── */
    @props {
        char  *title;               /* string */
        int    count;               /* integer */
        float  price;               /* float */
        void (*onAction)(void);     /* callback */
    }

    /* ── State: mutable, triggers re-render on change ──────────────── */
    @state {
        int  active = 0;            /* default value */
        int  qty    = 1;
    }

    /* ── Computed: derived values (recalculated on every refresh) ───── */
    @computed {
        char *label     = forge_sprintf("%s (%d)", props.title, props.count);
        char *price_str = forge_sprintf("$%.2f", props.price);
        int   has_item  = props.count > 0;
    }

    /* ── Event handlers ─────────────────────────────────────────────── */
    @on(toggle) {
        state.active = !state.active;
    }

    @on(inc) {
        state.qty = state.qty + 1;
    }

    /* ── Template: declarative HTML with reactive expressions ────────── */
    @template {
        <div class={state.active ? "card active" : "card"}>
            <h2>{computed.label}</h2>
            <p>{computed.price_str}</p>

            <if condition={computed.has_item}>
                <span class="badge">{props.count} items</span>
            </if>

            <for each={state.list} as={item}>
                <div>{item.name}</div>
            </for>

            <button onclick={@toggle}>Toggle</button>
            <button onclick={@inc}>+</button>

            <ChildComponent title={props.title} count={state.qty} />
        </div>
    }
}
```

### Supported Types in `@props` / `@state`

| C Type    | JS Equivalent | Notes                           |
|-----------|---------------|---------------------------------|
| `char *`  | `string`      | Text values                     |
| `int`     | `number`      | Integer values                  |
| `float`   | `number`      | Decimal values (use `%.2f`)     |
| `void *`  | `any`         | Arrays/objects passed from HTML |

### `forge_sprintf` Format Specifiers

| Format  | JS Output                      |
|---------|--------------------------------|
| `%s`    | `${value}`                     |
| `%d`    | `${Math.floor(value)}`         |
| `%f`    | `${value.toFixed(2)}`          |
| `%.2f`  | `${value.toFixed(2)}`  ← price |
| `%.0f`  | `${value.toFixed(0)}`          |
| `%05d`  | `${Math.floor(value)}`         |

### Known Parser Limitations

- **No apostrophes in template text** — `world's` breaks the lexer. Use `world` or an entity.
- **No HTML comments** inside `@template` — `<!-- ... -->` causes parse errors.
- **No CSS variables** in inline `style=` — `style="color: var(--x)"` fails. Use a `class` instead.
- **No complex method calls** in `<if condition>` — avoid `arr.includes("x")`, use a computed.

---

## 3. Compiling Components

### Basic compilation (no-wasm mode)

```bash
./build/forge compile --no-wasm -o dist MyComponent.cx
```

**Output:**
- `dist/MyComponent.forge.js` — Custom Element + DOM renderer
- `dist/MyComponent.forge.d.ts` — TypeScript declarations

### With SSG pre-rendering

```bash
./build/forge compile --no-wasm --prerender -o dist App.cx Child.cx
```

**Additional output:**
- `dist/App.forge.html` — static HTML snapshot for SEO/initial paint

### With SSR renderer

```bash
./build/forge compile --no-wasm --prerender --ssr -o dist App.cx Child.cx
```

**Additional output:**
- `dist/App.forge.ssr.js` — Node.js `render(state, props) => HTML` function

### Multi-component project (typical)

```bash
./build/forge compile --no-wasm --prerender --ssr \
    -o examples/myapp/dist \
    examples/myapp/ProductCard.cx \
    examples/myapp/ProductDetail.cx \
    examples/myapp/App.cx        # root component last
```

> **Order matters**: List child components before parent components.

---

## 4. Building an Application

### Directory Structure

```
myapp/
├── App.cx                    # root component
├── ProductCard.cx            # child component
├── index.html                # → generated by inject.py / SSR server
├── base_index.html           # HTML shell template (edit this)
├── api/
│   └── products.json         # static data for SSG
├── inject.py                 # SSG build script
├── ssr-server.js             # Node.js SSR server (optional)
└── dist/                     # compiler output
    ├── App.forge.js
    ├── App.forge.ssr.js
    ├── App.forge.html
    └── ProductCard.forge.js
```

### `base_index.html` Shell

This is the HTML wrapper that loads your compiled components. The `<forge-app>` tag is where your root component mounts.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>My App</title>
  <!-- styles here -->
</head>
<body>

<!-- Root component mounts here -->
<forge-app id="app"></forge-app>

<!-- PRODUCTS_DATA_PLACEHOLDER -->   <!-- inject.py replaces this -->

<!-- Compiled Forge components (order: children first, root last) -->
<script type="module" src="dist/ProductCard.forge.js"></script>
<script type="module" src="dist/App.forge.js"></script>

<!-- App bootstrap module -->
<script type="module">
  'use strict';
  const app = document.getElementById('app');

  // Load data and set on component
  const dataEl = document.getElementById('myapp-data');
  if (dataEl) {
    app.products = JSON.parse(dataEl.textContent);
  }
</script>
</body>
</html>
```

### The Generated Custom Element API

After compilation, each component becomes a Custom Element. The root `<forge-app>` exposes:

```javascript
const app = document.getElementById('app');

// Set props/state from outside
app.products = [...];        // triggers _refresh()
app.page = 2;                // triggers _refresh()
app.selected_id = 42;        // triggers _refresh()

// Read current state
app._state.page              // current page number
app._state.products          // current products array

// Manually trigger re-render
app._refresh();

// Override a setter (intercept state changes)
Object.defineProperty(app, 'filter_tag', {
  get() { return this._state.filter_tag; },
  set(val) {
    this._state.filter_tag = val;
    this._state.products = filterProducts(val);
    this._refresh();
  },
  configurable: true,
});

// Patch _refresh to hook page navigation
const orig = app._refresh.bind(app);
app._refresh = function() {
  orig();
  updatePageTitle(this._state.page);
};
```

---

## 5. Routing (History API)

Forge uses state-based routing: a single `state.page` integer controls which page is shown. The client-side router maps URL paths ↔ page numbers.

### `base_index.html` Router Setup

```javascript
/* ── Route maps ─────────────────────────────────────────────────── */
const PAGE_PATHS = ['/', '/products', '/product', '/cart', '/checkout'];
const PATH_PAGE  = { '/': 0, '/home': 0, '/products': 1, '/cart': 3 };
const PAGE_TITLE = ['My App — Home', 'My App — Products', ...];

let _routerBusy = false;

/* Push URL without triggering applyRoute */
function pushRoute(page, selectedId) {
  if (_routerBusy) return;
  const path = page === 2
    ? '/product/' + (selectedId || '')
    : (PAGE_PATHS[page] || '/');
  if (window.location.pathname !== path)
    history.pushState(null, '', path);
  document.title = PAGE_TITLE[page] || 'My App';
}

/* Apply a URL path to Forge state */
function applyRoute(path) {
  path = path || '/';
  const m = path.match(/^\/product\/(\d+)$/);
  _routerBusy = true;
  if (m) {
    app._state.selected_id = +m[1];
    app._state.page = 2;
    app._refresh();
  } else {
    const page = PATH_PAGE[path];
    if (page !== undefined) {
      app._state.page = page;
      app._refresh();
    }
  }
  _routerBusy = false;
}

/* Browser back/forward */
window.addEventListener('popstate', () => applyRoute(window.location.pathname));

/* SPA link interceptor — prevents full page reload */
document.addEventListener('click', e => {
  const a = e.target.closest('a[href]');
  if (!a) return;
  let url;
  try { url = new URL(a.href, location.origin); } catch { return; }
  if (url.origin !== location.origin) return;
  e.preventDefault();
  if (url.pathname === location.pathname) return;
  history.pushState(null, '', url.pathname);
  applyRoute(url.pathname);
});

/* Patch _refresh to push URL on page changes */
customElements.whenDefined('forge-app').then(() => {
  let _lastPage = app._state.page;
  const orig = app._refresh.bind(app);
  app._refresh = function() {
    orig();
    const pg = this._state.page;
    if (pg !== _lastPage) {
      _lastPage = pg;
      pushRoute(pg, this._state.selected_id);
    }
  };

  /* Apply initial URL on load */
  if (window.location.pathname !== '/')
    applyRoute(window.location.pathname);
});
```

### Adding `href` to Nav Links in `.cx`

Always include `href` on `<a>` tags for accessibility and "open in new tab":

```c
@template {
    <nav>
        <a href="/" class={state.page == 0 ? "nav-link active" : "nav-link"} onclick={@go_home}>Home</a>
        <a href="/products" class={state.page == 1 ? "nav-link active" : "nav-link"} onclick={@go_products}>Products</a>
        <a href="/cart" class="nav-cart" onclick={@go_cart}>Cart</a>
    </nav>
}
```

The SPA link interceptor prevents actual navigation when JS is loaded; the `onclick` handlers update Forge state directly for instant feedback.

### Dev Server SPA Fallback

For local development, the `forge-dev` server serves `index.html` for all unknown paths so direct URL access works:

```bash
./build/forge-dev --dir examples/myapp --port 8000
# Now /products, /cart, /product/5 all serve index.html correctly
```

---

## 6. SSG — Static Site Generation

SSG pre-renders the component to a static HTML file at build time and injects it into `index.html`. This means users see content **before any JavaScript loads**.

### Step 1: Compile with `--prerender`

```bash
./build/forge compile --no-wasm --prerender -o dist App.cx
```

This generates `dist/App.forge.html` — a static HTML snapshot of the component's initial state.

### Step 2: Create `inject.py`

```python
#!/usr/bin/env python3
import json, re
from pathlib import Path

BASE = Path(__file__).parent
DIST = BASE / 'dist'
API  = BASE / 'api' / 'products.json'

products = json.loads(API.read_text())
html     = (DIST / 'App.forge.html').read_text()
shell    = (BASE / 'base_index.html').read_text()

# Inject SSG HTML into forge-app
shell = re.sub(
    r'<forge-app id="app">.*?</forge-app>',
    f'<forge-app id="app">{html}</forge-app>',
    shell, flags=re.DOTALL
)

# Inject product data JSON
data_script = (
    '<script id="myapp-data" type="application/json">\n'
    + json.dumps(products, indent=2)
    + '\n</script>'
)
shell = shell.replace('<!-- PRODUCTS_DATA_PLACEHOLDER -->', data_script)

(BASE / 'index.html').write_text(shell)
print(f'✓ SSG build complete → {BASE}/index.html')
```

```bash
python3 inject.py
# → generates index.html with pre-rendered HTML + inlined data
```

---

## 7. SSR — Server-Side Rendering

True SSR renders HTML on the server **on every request** — supports dynamic data and HTTP streaming.

### Step 1: Compile with `--ssr`

```bash
./build/forge compile --no-wasm --prerender --ssr -o dist App.cx
# → generates dist/App.forge.ssr.js
```

### Step 2: The Generated SSR Module

`App.forge.ssr.js` exports a `render(state, props) => string` function:

```javascript
const { render } = require('./dist/App.forge.ssr.js');

const html = render({
  page: 1,
  products: [...],
  filter_tag: 'all',
  cart_count: 0,
  selected_id: null,
}, {});

// html is a complete HTML string of the component
```

### Step 3: Streaming SSR Server

```javascript
// ssr-server.js
const http = require('http');
const fs   = require('fs');
const path = require('path');

const { render }  = require('./dist/App.forge.ssr.js');
const BASE_HTML   = fs.readFileSync('./base_index.html', 'utf8');
const SPLIT_TAG   = '<forge-app id="app">';
const SPLIT_IDX   = BASE_HTML.indexOf(SPLIT_TAG);
const SHELL_HEAD  = BASE_HTML.slice(0, SPLIT_IDX + SPLIT_TAG.length);
const SHELL_TAIL  = BASE_HTML.slice(SPLIT_IDX + SPLIT_TAG.length);

http.createServer((req, res) => {
  const urlPath = (req.url || '/').split('?')[0];

  // Static assets
  if (path.extname(urlPath)) {
    const data = fs.readFileSync(path.join(__dirname, urlPath));
    res.writeHead(200);
    res.end(data);
    return;
  }

  // SSR
  const products = JSON.parse(fs.readFileSync('./api/products.json', 'utf8'));
  const state    = { page: 0, products, filter_tag: 'all', cart_count: 0 };
  const ssrHtml  = render(state, {});
  const dataScript = `<script id="myapp-data" type="application/json">
${JSON.stringify(products)}
</script>`;

  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });

  // Stream in chunks: shell → SSR HTML → scripts
  res.write(SHELL_HEAD);     // <html>...<forge-app id="app">
  res.write(ssrHtml);        // SSR content (browser paints this NOW)
  res.write('</forge-app>');

  // IMPORTANT: sync clear script so Forge mounts fresh (not into SSR content)
  res.write('<script>document.getElementById("app").innerHTML="";</script>\n');

  const shellTail = SHELL_TAIL.replace('<!-- PRODUCTS_DATA_PLACEHOLDER -->', dataScript);
  res.write(shellTail);      // scripts + bootstrap JS
  res.end();

}).listen(3001, () => console.log('SSR server on http://localhost:3001'));
```

```bash
node ssr-server.js
```

### How Streaming SSR Works

```
Browser                          Server
  |                                |
  | GET /products                  |
  |─────────────────────────────→  |
  |                                | reads products.json (live data)
  |  Chunk 1: <html>...<forge-app> | calls render({page:1, products})
  |←────────────────────────────── | streams immediately
  |  [browser paints navbar]       |
  |  Chunk 2: SSR product cards    |
  |←────────────────────────────── |
  |  [browser paints products]     |
  |  Chunk 3: <script> tags        |
  |←────────────────────────────── |
  |  [JS hydrates, links work]     |
```

---

## 8. SEO — Dynamic Meta Tags

Update `<title>`, `<meta description>`, and Open Graph tags on every page navigation.

### Add to `base_index.html` (inside the bootstrap `<script type="module">`)

```javascript
/* ── SEO config ──────────────────────────────────────────────────── */
const SEO = {
  0: {
    title:       'MyShop — Home',
    description: 'Shop 20,000+ products. Free shipping over $50.',
    og_image:    '/og/home.jpg',
  },
  1: {
    title:       'MyShop — All Products',
    description: 'Browse our full catalog of electronics, clothing, books and more.',
    og_image:    '/og/products.jpg',
  },
  2: {
    title:       (product) => `${product.name} — MyShop`,
    description: (product) => `${product.name}. ${product.description || ''}`.slice(0, 160),
    og_image:    (product) => product.image_url || '/og/default.jpg',
  },
  3: {
    title:       'MyShop — Cart',
    description: 'Review your cart and proceed to checkout.',
    og_image:    '/og/cart.jpg',
  },
};

function updateSEO(page, product) {
  const cfg = SEO[page] || SEO[0];
  const title = typeof cfg.title === 'function' ? cfg.title(product || {}) : cfg.title;
  const desc  = typeof cfg.description === 'function' ? cfg.description(product || {}) : cfg.description;
  const img   = typeof cfg.og_image === 'function' ? cfg.og_image(product || {}) : cfg.og_image;

  document.title = title;

  // <meta name="description">
  let metaDesc = document.querySelector('meta[name="description"]');
  if (!metaDesc) {
    metaDesc = document.createElement('meta');
    metaDesc.name = 'description';
    document.head.appendChild(metaDesc);
  }
  metaDesc.content = desc;

  // Open Graph
  const og = { 'og:title': title, 'og:description': desc, 'og:image': img };
  for (const [prop, val] of Object.entries(og)) {
    let el = document.querySelector(`meta[property="${prop}"]`);
    if (!el) {
      el = document.createElement('meta');
      el.setAttribute('property', prop);
      document.head.appendChild(el);
    }
    el.content = val || '';
  }

  // Canonical URL
  let canonical = document.querySelector('link[rel="canonical"]');
  if (!canonical) {
    canonical = document.createElement('link');
    canonical.rel = 'canonical';
    document.head.appendChild(canonical);
  }
  canonical.href = window.location.origin + window.location.pathname;
}

/* Patch _refresh to update SEO on every navigation */
customElements.whenDefined('forge-app').then(() => {
  const orig = app._refresh.bind(app);
  app._refresh = function() {
    orig();
    const page = this._state.page;
    const pid  = this._state.selected_id;
    const prod = page === 2 && pid
      ? allProducts.find(p => p.id === pid)
      : null;
    updateSEO(page, prod);
    pushRoute(page, pid);   /* also update URL */
  };

  /* Initial SEO for current page on load */
  updateSEO(app._state.page, null);
});
```

### SSR + SEO (Server-Side)

For full SEO (crawlers see pre-rendered meta tags), update `ssr-server.js` to inject dynamic meta into `SHELL_HEAD`:

```javascript
function buildHead(state, products) {
  const product = state.page === 2
    ? products.find(p => p.id === state.selected_id)
    : null;

  const title = product
    ? `${product.name} — MyShop`
    : ({ 0: 'MyShop — Home', 1: 'MyShop — Products' })[state.page] || 'MyShop';

  const desc = product
    ? `${product.name}. Buy online with fast delivery.`
    : 'Shop 20,000+ products at the best prices.';

  return `<title>${title}</title>
    <meta name="description" content="${desc}">
    <meta property="og:title" content="${title}">
    <meta property="og:description" content="${desc}">`;
}

// In handleSSR():
const headTags = buildHead(state, products);
const shellWithHead = SHELL_HEAD.replace('<title>My App</title>', headTags);
res.write(shellWithHead);
```

---

## 9. API Integration & Data Loading

### Pattern: Fetch on Mount, SSG as Fallback

```javascript
// In base_index.html bootstrap
let allProducts = [];

/* 1. SSG data loads instantly (pre-injected at build time) */
const dataEl = document.getElementById('myapp-data');
if (dataEl) {
  allProducts = JSON.parse(dataEl.textContent);
  app.products = allProducts;
}

/* 2. Background API refresh for live data */
(async () => {
  try {
    const res = await fetch('/api/products.json');
    if (res.ok) {
      allProducts = await res.json();
      app.products = allProducts;
    }
  } catch { /* SSG data is the fallback */ }
})();
```

### Paginated API (load-more pattern)

```javascript
let page = 1;
let loading = false;

async function loadMore() {
  if (loading) return;
  loading = true;
  const res = await fetch(`/api/medicines/?limit=20&offset=${(page-1)*20}`);
  const data = await res.json();
  allProducts = [...allProducts, ...data.results];
  app.products = allProducts;
  app.total_count = data.count;
  page++;
  loading = false;
}

window.addEventListener('forge:load-more', loadMore);
loadMore(); // initial load
```

### Authenticated API

```javascript
const TOKEN = localStorage.getItem('auth_token');

async function apiFetch(endpoint) {
  const res = await fetch(`http://localhost:8000/api${endpoint}`, {
    headers: {
      'Authorization': `Bearer ${TOKEN}`,
      'Accept': 'application/json',
    }
  });
  if (!res.ok) throw new Error(`API ${res.status}: ${endpoint}`);
  return res.json();
}

// Load categories + products
const [categories, medicines] = await Promise.all([
  apiFetch('/categories/'),
  apiFetch('/medicines/?limit=20'),
]);
app.categories = categories.results;
app.products = medicines.results;
```

---

## 10. Component Communication

### Child → Parent (Custom Events)

In the `.cx` event handler:
```c
@on(add_to_cart) {
    window.dispatchEvent(new CustomEvent("forge:add-to-cart", {
        detail: { id: this._props.product_id, qty: this._state.qty }
    }));
}
```

In `base_index.html`:
```javascript
window.addEventListener('forge:add-to-cart', e => {
  Cart.add(e.detail.id, e.detail.qty);
  app.cart_count = Cart.count();
  toast('Added to cart!', 'success');
});
```

### Parent → Child (Props)

```javascript
// Setting props from outside triggers child re-render
childElement.product_id = 42;
childElement.price = 29.99;
```

### Cross-Component via Root State

```javascript
// Any page transition sets app state → all children re-render
app.page = 2;
app.selected_id = 42;
// _refresh() is called automatically by the setter
```

---

## 11. Cart & Local State

### localStorage Cart

```javascript
const Cart = {
  _key: 'myapp_cart',
  get()       { return JSON.parse(localStorage.getItem(this._key)) || []; },
  save(items) { localStorage.setItem(this._key, JSON.stringify(items)); },
  add(id, qty = 1) {
    const items = this.get();
    const ex = items.find(i => i.id === id);
    if (ex) ex.qty += qty; else items.push({ id, qty });
    this.save(items);
  },
  remove(id)         { this.save(this.get().filter(i => i.id !== id)); },
  setQty(id, qty)    { if (qty <= 0) { this.remove(id); return; }
                        const items = this.get();
                        const it = items.find(i => i.id === id);
                        if (it) it.qty = qty; this.save(items); },
  count()            { return this.get().reduce((s, i) => s + i.qty, 0); },
  subtotal(products) { return this.get().reduce((s, i) => {
                          const p = products.find(x => x.id === i.id);
                          return p ? s + p.price * i.qty : s; }, 0); },
};
```

---

## 12. Deployment

### Static Hosting (SSG)

```bash
make examples          # compiles all .cx files + runs inject.py
# Deploy the example directory (index.html + dist/)
```

Server must serve `index.html` for all unknown paths (SPA fallback):

**Nginx:**
```nginx
location / {
  try_files $uri $uri/ /index.html;
}
```

**Express.js:**
```javascript
app.get('*', (req, res) => res.sendFile(path.join(__dirname, 'index.html')));
```

### SSR Deployment (Node.js)

```bash
node examples/myapp/ssr-server.js --port=3001
```

**Behind Nginx:**
```nginx
location / {
  proxy_pass http://localhost:3001;
  proxy_set_header Host $host;
}
```

### Environment: Production vs Development

| Feature            | SSG (`index.html`)      | SSR (`ssr-server.js`)        |
|--------------------|-------------------------|------------------------------|
| Initial HTML       | Pre-built at build time | Generated on every request   |
| Dynamic data       | No (stale until rebuild)| Yes (fresh on every request) |
| SEO               | Good (static snapshot)  | Perfect (dynamic per-page)   |
| Hosting            | Any static host (CDN)   | Node.js server needed        |
| Dev server         | `forge-dev` or any HTTP | `node ssr-server.js`         |

---

## 13. Complete Example Walkthrough

### Step-by-step: Create a Product Listing App

```bash
mkdir -p examples/myshop/api examples/myshop/dist
```

**1. `examples/myshop/ProductCard.cx`**

```c
@component ProductCard {
    @props {
        int    product_id;
        char  *name;
        float  price;
        char  *emoji;
    }
    @computed {
        char *price_str = forge_sprintf("$%.2f", props.price);
    }
    @on(view) {
        window.dispatchEvent(new CustomEvent("forge:view-product", {
            detail: { id: this._props.product_id }
        }));
    }
    @template {
        <div class="card" onclick={@view}>
            <div class="emoji">{props.emoji}</div>
            <h3>{props.name}</h3>
            <p>{computed.price_str}</p>
        </div>
    }
}
```

**2. `examples/myshop/App.cx`**

```c
#include "ProductCard.cx"

@component App {
    @state {
        int    page       = 0;
        int    selected_id = 0;
        void  *products;
    }
    @on(go_home)     { state.page = 0; }
    @on(go_products) { state.page = 1; }
    @template {
        <div>
            <nav>
                <a href="/" onclick={@go_home}>Home</a>
                <a href="/products" onclick={@go_products}>Products</a>
            </nav>
            <if condition={state.page == 0}>
                <h1>Welcome</h1>
            </if>
            <if condition={state.page == 1}>
                <for each={state.products} as={p}>
                    <ProductCard product_id={p.id} name={p.name} price={p.price} emoji={p.emoji} />
                </for>
            </if>
        </div>
    }
}
```

**3. Compile**

```bash
./build/forge compile --no-wasm --prerender --ssr \
    -o examples/myshop/dist \
    examples/myshop/ProductCard.cx \
    examples/myshop/App.cx
```

**4. Create `examples/myshop/api/products.json`**

```json
[
  { "id": 1, "name": "Headphones", "price": 49.99, "emoji": "🎧" },
  { "id": 2, "name": "Keyboard",   "price": 89.00, "emoji": "⌨️" }
]
```

**5. Create `examples/myshop/base_index.html`** ← copy from examples/05-ecommerce and customize

**6. Run `inject.py`** ← copy from examples/05-ecommerce and adapt paths

**7. Start dev server**

```bash
./build/forge-dev --dir examples/myshop --port 8000
# open http://localhost:8000
```

---

## Appendix: Compiler Flags Reference

| Flag | Description |
|------|-------------|
| `--no-wasm` | Generate pure-JS renderer (skip Clang). Required unless WASM toolchain installed. |
| `--prerender` | Generate static HTML snapshot (`*.forge.html`) for SSG. |
| `--ssr` | Generate Node.js SSR renderer (`*.forge.ssr.js`). |
| `-o <dir>` | Output directory (default: `./dist`). |
| `--ast` | Dump parsed AST to stdout (debug). |
| `--no-types` | Skip TypeScript `.d.ts` output. |
| `--iife` | Emit IIFE JS instead of ES modules. |
| `--no-web-comp` | Skip `customElements.define`. |
| `-g` | Emit debug info (WASM only). |

## Appendix: Makefile Integration

```makefile
myapp: compiler
	$(BUILD_DIR)/forge compile --no-wasm --prerender --ssr \
	    -o examples/myapp/dist \
	    examples/myapp/ProductCard.cx \
	    examples/myapp/App.cx
	python3 examples/myapp/inject.py
```

Add `myapp` to the `examples` target dependencies.
