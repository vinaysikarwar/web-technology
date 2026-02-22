#!/usr/bin/env node
'use strict';
/**
 * Forge SSR Server — examples/05-ecommerce/ssr-server.js
 *
 * True Server-Side Rendering with HTTP streaming.
 * On every HTML request:
 *   1. Reads fresh product data from api/products.json (dynamic, not cached)
 *   2. Determines page from URL path (same routes as client router)
 *   3. Calls render(state) from App.forge.ssr.js → HTML string
 *   4. Streams response in chunks:
 *        Chunk 1 → HTML shell up to <forge-app>
 *        Chunk 2 → SSR-rendered component HTML  (browser shows content NOW)
 *        Chunk 3 → Rest of shell (scripts, bootstrap JS)
 *      Browser receives JS and hydrates — client router takes over.
 *
 * Run:
 *   node examples/05-ecommerce/ssr-server.js
 *   node examples/05-ecommerce/ssr-server.js --port=4000
 */

const http = require('http');
const fs   = require('fs');
const path = require('path');

/* ─── Config ──────────────────────────────────────────────────────────────── */
const DIR  = __dirname;
const PORT = (() => {
  const a = process.argv.find(x => x.startsWith('--port='));
  return a ? parseInt(a.split('=')[1]) : 3001;
})();

/* ─── Load SSR renderer (compiled by: forge compile --ssr) ───────────────── */
const SSR_FILE = path.join(DIR, 'dist', 'App.forge.ssr.js');
if (!fs.existsSync(SSR_FILE)) {
  console.error(`\n  Error: ${SSR_FILE} not found.`);
  console.error('  Run first:  make examples  (builds with --ssr flag)\n');
  process.exit(1);
}
const { render } = require(SSR_FILE);

/* ─── Load & split HTML shell for streaming ──────────────────────────────── */
/*
 * base_index.html is split at the <forge-app id="app"> tag so we can
 * stream:  shell-before → SSR HTML → shell-after (with scripts).
 * This means the browser displays content BEFORE any JS is parsed.
 */
const BASE_HTML  = fs.readFileSync(path.join(DIR, 'base_index.html'), 'utf8');
const SPLIT_TAG  = '<forge-app id="app">';
const SPLIT_IDX  = BASE_HTML.indexOf(SPLIT_TAG);
const SHELL_HEAD = SPLIT_IDX >= 0
  ? BASE_HTML.slice(0, SPLIT_IDX + SPLIT_TAG.length)
  : BASE_HTML;
const SHELL_TAIL = SPLIT_IDX >= 0
  ? BASE_HTML.slice(SPLIT_IDX + SPLIT_TAG.length)
  : '';

/* ─── MIME map for static assets ──────────────────────────────────────────── */
const MIME = {
  '.js':   'application/javascript; charset=utf-8',
  '.json': 'application/json',
  '.css':  'text/css',
  '.html': 'text/html; charset=utf-8',
  '.wasm': 'application/wasm',
  '.ico':  'image/x-icon',
  '.png':  'image/png',
  '.svg':  'image/svg+xml',
};

/* ─── URL path → page number ─────────────────────────────────────────────── */
const PATH_PAGE = {
  '/': 0, '/home': 0,
  '/products': 1,
  /* /product/:id → 2 (handled separately) */
  '/cart': 3,
  '/checkout': 4,
  '/about': 5,
  '/contact': 6,
};

function stateFromPath(urlPath, products) {
  /* Product detail: /product/5 */
  const m = urlPath.match(/^\/product\/(\d+)$/);
  if (m) {
    return {
      page: 2,
      selected_id: +m[1],
      products,
      filter_tag: 'all',
      cart_count: 0,
    };
  }
  const page = PATH_PAGE[urlPath] ?? 0;
  return { page, products, filter_tag: 'all', cart_count: 0, selected_id: null };
}

/* ─── Dynamic SEO meta tags ───────────────────────────────────────────────── */
const SEO_META = {
  0: { title: 'ShopForge — Home',     desc: 'Shop 20,000+ products. Free shipping over $50.' },
  1: { title: 'ShopForge — Products', desc: 'Browse electronics, clothing, books, home goods and more.' },
  3: { title: 'ShopForge — Cart',     desc: 'Review your cart and proceed to checkout.' },
  4: { title: 'ShopForge — Checkout', desc: 'Complete your purchase securely.' },
  5: { title: 'ShopForge — About',    desc: 'Learn about ShopForge.' },
  6: { title: 'ShopForge — Contact',  desc: 'Get in touch with ShopForge.' },
};

function buildSeoHead(state, products) {
  let title, desc;
  if (state.page === 2 && state.selected_id) {
    const p = products.find(x => x.id === state.selected_id);
    title = p ? `${p.name} — ShopForge` : 'Product — ShopForge';
    desc  = p ? (p.description || p.name).slice(0, 155) : 'View product details.';
  } else {
    const cfg = SEO_META[state.page] || SEO_META[0];
    title = cfg.title;
    desc  = cfg.desc;
  }
  return { title, desc };
}

/* ─── SSR request handler ─────────────────────────────────────────────────── */
function handleSSR(req, res, urlPath) {
  /* 1. Read FRESH product data on every request (dynamic data) */
  let products;
  try {
    products = JSON.parse(fs.readFileSync(path.join(DIR, 'api', 'products.json'), 'utf8'));
  } catch (err) {
    res.writeHead(500, { 'Content-Type': 'text/plain' });
    res.end('SSR: could not read products.json — ' + err.message);
    return;
  }

  /* 2. Determine page state from URL */
  const state = stateFromPath(urlPath, products);

  /* 3. Render component to HTML string */
  let ssrHtml;
  try {
    ssrHtml = render(state, {});
  } catch (err) {
    res.writeHead(500, { 'Content-Type': 'text/plain' });
    res.end('SSR render error: ' + err.message + '\n' + err.stack);
    return;
  }

  /* 4. Inline product data script (same as SSG inject.py does at build time,
   *    but done dynamically here on every request) */
  const dataScript =
    `<script id="shopforge-products-data" type="application/json">\n` +
    JSON.stringify(products, null, 2) +
    `\n</script>`;

  /* Inject data script into shell tail (replaces placeholder if present,
   * otherwise prepend before </body>) */
  const PLACEHOLDER = '<!-- PRODUCTS_DATA_PLACEHOLDER -->';
  const shellTail = SHELL_TAIL.includes(PLACEHOLDER)
    ? SHELL_TAIL.replace(PLACEHOLDER, dataScript)
    : SHELL_TAIL.replace('</body>', dataScript + '\n</body>');

  /* 5. Build dynamic SEO head — replace <title> + meta tags in the shell */
  const { title, desc } = buildSeoHead(state, products);
  const shellHead = SHELL_HEAD
    .replace(/<title>[^<]*<\/title>/, `<title>${title}</title>`)
    .replace(/(<meta name="description" content=")[^"]*(")/,  `$1${desc}$2`)
    .replace(/(<meta property="og:title" content=")[^"]*(")/,   `$1${title}$2`)
    .replace(/(<meta property="og:description" content=")[^"]*(")/,  `$1${desc}$2`)
    .replace(/(<meta name="twitter:title" content=")[^"]*(")/,  `$1${title}$2`)
    .replace(/(<link rel="canonical" href=")[^"]*(")/,
      `$1${urlPath === '/' ? '/' : urlPath}$2`);

  /* 6. Stream in 3 chunks — Transfer-Encoding: chunked is implicit with res.write */
  res.writeHead(200, {
    'Content-Type': 'text/html; charset=utf-8',
    'Cache-Control': 'no-store',
    'X-Forge-SSR': 'true',
    'X-Forge-Page': String(state.page),
  });

  res.write(shellHead);           /* Chunk 1: <html>...<forge-app id="app"> (dynamic SEO) */
  res.write(ssrHtml);             /* Chunk 2: SSR-rendered component markup     */
  res.write('</forge-app>');      /* Close the tag (shell-tail starts AFTER it) */
  /* Sync script: runs immediately (before ES modules) and clears forge-app so
   * Forge mounts into an empty element (_hydrate = false) and properly attaches
   * event listeners. The SSR content above was already painted by the browser. */
  res.write('<script>document.getElementById("app").innerHTML="";</script>\n');
  res.write(shellTail);           /* Chunk 3: scripts + bootstrap JS            */
  res.end();
}

/* ─── Request router ──────────────────────────────────────────────────────── */
const server = http.createServer((req, res) => {
  const urlPath = (req.url || '/').split('?')[0].replace(/\/$/, '') || '/';
  const ext = path.extname(urlPath);

  /* Static assets (JS, JSON, CSS, fonts, etc.) */
  if (ext && MIME[ext]) {
    const filePath = path.join(DIR, urlPath);
    try {
      const data = fs.readFileSync(filePath);
      res.writeHead(200, {
        'Content-Type': MIME[ext],
        'Cache-Control': 'public, max-age=60',
      });
      res.end(data);
    } catch {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('Not Found: ' + urlPath);
    }
    return;
  }

  /* All other routes → SSR */
  handleSSR(req, res, urlPath);
});

/* ─── Start ───────────────────────────────────────────────────────────────── */
server.listen(PORT, () => {
  const url = `http://localhost:${PORT}`;
  console.log(`\n  \x1b[32mForge SSR Server\x1b[0m  v0.1.0`);
  console.log(`  \x1b[36mLocal:\x1b[0m    ${url}`);
  console.log(`  \x1b[36mMode:\x1b[0m     Server-Side Rendering + HTTP streaming`);
  console.log(`  \x1b[36mData:\x1b[0m     api/products.json  (fresh on every request)`);
  console.log(`  \x1b[36mRenderer:\x1b[0m dist/App.forge.ssr.js`);
  console.log(`\n  Routes:`);
  console.log(`    ${url}/             → Home`);
  console.log(`    ${url}/products     → All Products`);
  console.log(`    ${url}/product/1    → Product Detail`);
  console.log(`    ${url}/cart         → Cart`);
  console.log(`    ${url}/checkout     → Checkout`);
  console.log(`    ${url}/about        → About`);
  console.log(`    ${url}/contact      → Contact`);
  console.log(`\n  Press Ctrl+C to stop\n`);
});
