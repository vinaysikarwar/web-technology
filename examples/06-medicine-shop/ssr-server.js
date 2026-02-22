#!/usr/bin/env node
'use strict';
/**
 * MediForge SSR Server — examples/06-medicine-shop/ssr-server.js
 *
 * Server-Side Rendering + API proxy for the medicine shop.
 *
 * On each page request:
 *   - Fetches real data from the Django API
 *   - Injects it as readable HTML into <forge-app> (visible in view-source)
 *   - Injects window.__SSR_DATA__ JSON so Forge avoids redundant API calls
 *   - Injects a tiny sync script that clears the SSR content right before
 *     Forge mounts (prevents duplicate/conflict content)
 *   - Updates <title> / <meta> tags per route
 *
 * Usage:
 *   node ssr-server.js [--port=3002] [--api=http://localhost:8000] [--token=<jwt>]
 *
 * Or set env vars:
 *   MEDIFORGE_API=http://localhost:8000 MEDIFORGE_TOKEN=eyJ... node ssr-server.js
 */

const http   = require('http');
const https  = require('https');
const fs     = require('fs');
const path   = require('path');
const urlMod = require('url');

/* ── Config ────────────────────────────────────────────────────────────────── */
const DIR = __dirname;

const PORT = (() => {
  const a = process.argv.find(x => x.startsWith('--port='));
  return a ? parseInt(a.split('=')[1]) : 3002;
})();

const API_BASE = (() => {
  const a = process.argv.find(x => x.startsWith('--api='));
  return (a ? a.split('=').slice(1).join('=') : null)
      || process.env.MEDIFORGE_API
      || 'http://localhost:8000';
})();

const API_TOKEN = (() => {
  const a = process.argv.find(x => x.startsWith('--token='));
  return (a ? a.split('=').slice(1).join('=') : null)
      || process.env.MEDIFORGE_TOKEN
      || '';
})();

/* ── MIME ──────────────────────────────────────────────────────────────────── */
const MIME = {
  '.js':   'application/javascript; charset=utf-8',
  '.mjs':  'application/javascript; charset=utf-8',
  '.json': 'application/json',
  '.css':  'text/css; charset=utf-8',
  '.html': 'text/html; charset=utf-8',
  '.png':  'image/png',
  '.jpg':  'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.svg':  'image/svg+xml',
  '.ico':  'image/x-icon',
  '.woff2':'font/woff2',
};

/* ── Internal API fetch (Node → Django) ────────────────────────────────────── */
function nodeFetch(endpoint) {
  return new Promise((resolve, reject) => {
    const fullUrl = API_BASE + endpoint;
    const parsed  = new urlMod.URL(fullUrl);
    const isHttps = parsed.protocol === 'https:';
    const transport = isHttps ? https : http;

    const options = {
      hostname: parsed.hostname,
      port:     parsed.port || (isHttps ? 443 : 80),
      path:     parsed.pathname + parsed.search,
      method:   'GET',
      headers: {
        'Accept':       'application/json',
        'Content-Type': 'application/json',
        ...(API_TOKEN ? { 'Authorization': 'Bearer ' + API_TOKEN } : {}),
      },
    };

    const req = transport.request(options, res => {
      let body = '';
      res.setEncoding('utf8');
      res.on('data', chunk => body += chunk);
      res.on('end', () => {
        try { resolve(JSON.parse(body)); }
        catch (e) { reject(new Error(`JSON parse failed for ${endpoint}: ${e.message}`)); }
      });
    });
    req.on('error', reject);
    req.setTimeout(8000, () => { req.destroy(new Error('SSR API timeout: ' + endpoint)); });
    req.end();
  });
}

/* ── Emoji map ──────────────────────────────────────────────────────────────── */
function getMedEmoji(dosage_form) {
  const map = {
    tablet:'💊', capsule:'💊', pill:'💊',
    syrup:'🧴', suspension:'🧴', liquid:'🧴', solution:'🧴',
    injection:'💉', vaccine:'💉',
    cream:'🧴', ointment:'🧴', gel:'🧴', lotion:'🧴',
    drops:'💧', 'nasal spray':'💧', 'eye drops':'💧',
    inhaler:'🫁', nebulizer:'🫁',
    powder:'🫙', granules:'🫙',
    patch:'🩹', bandage:'🩹',
  };
  return map[(dosage_form || '').toLowerCase()] || '💊';
}

/* ── HTML escape ─────────────────────────────────────────────────────────── */
function esc(s) {
  return String(s || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

/* ── Category card HTML ──────────────────────────────────────────────────── */
function catCardHtml(cat) {
  const img = cat.image
    ? `<img class="cat-img" src="${esc(cat.image)}" alt="${esc(cat.name)}" loading="lazy">`
    : `<span style="font-size:1.6rem">💊</span>`;
  return `<div class="cat-card">
  <div class="cat-img-wrap">${img}</div>
  <span class="cat-name">${esc(cat.name)}</span>
</div>`;
}

/* ── Medicine card HTML ───────────────────────────────────────────────────── */
function medCardHtml(m) {
  const emoji    = getMedEmoji(m.dosage_form);
  const catName  = m.category?.name || '';
  const price    = parseFloat(m.price || 0).toFixed(2);
  const rxBadge  = m.prescription_required
    ? `<span class="badge badge-rx">Rx</span>` : '';
  const detailUrl = `/medicine/${esc(m.slug || m.id)}`;
  return `<article class="med-card">
  <div class="med-card-top">
    <span class="med-emoji">${emoji}</span>
    <div class="med-badges">${rxBadge}</div>
  </div>
  <div class="med-card-body">
    <div class="med-cat-label">${esc(catName)}</div>
    <h3 class="med-name">${esc(m.name)}</h3>
    <div class="med-ingr">${esc(m.primary_ingredient || '')}</div>
    <div class="med-dosage-form">${esc(m.dosage_form || '')}</div>
  </div>
  <div class="med-card-footer">
    <span class="med-price">&#8377;${esc(price)}</span>
    <a href="${detailUrl}" class="add-btn">View</a>
  </div>
</article>`;
}

/* ── Shared navbar HTML ──────────────────────────────────────────────────── */
function navbarHtml(activePage) {
  const homeActive = activePage === 'home' ? ' active' : '';
  const listActive = activePage === 'list' ? ' active' : '';
  return `<nav class="navbar">
  <div class="nav-inner">
    <a href="/" class="nav-brand">
      <span class="nav-brand-icon">&#x1F48A;</span>
      <span class="nav-brand-name">MediForge</span>
    </a>
    <div class="nav-links">
      <a href="/" class="nav-link${homeActive}">Home</a>
      <a href="/medicines" class="nav-link${listActive}">Medicines</a>
      <a href="/cart" class="nav-cart">&#x1F6D2;</a>
    </div>
  </div>
</nav>`;
}

/* ── Footer HTML ─────────────────────────────────────────────────────────── */
function footerHtml() {
  return `<footer class="site-footer">
  <div class="footer-inner">&#169; 2026 MediForge. Built with Forge Framework.</div>
</footer>`;
}

/* ── Home page SSR ──────────────────────────────────────────────────────── */
async function buildHomeSSR() {
  let cats = [], meds = [], total = 0;
  try {
    const [catData, medData] = await Promise.all([
      nodeFetch('/api/categories/'),
      nodeFetch('/api/medicines/?is_discontinued=false&ordering=-created_at&limit=20'),
    ]);
    cats  = catData.results  || catData  || [];
    meds  = medData.results  || [];
    total = medData.count    || 0;
  } catch (e) {
    console.warn('[SSR] Home data fetch failed:', e.message);
  }

  const catCards = cats.map(catCardHtml).join('\n');
  const medCards = meds.map(medCardHtml).join('\n');

  const ssrHtml = `${navbarHtml('home')}
<div class="home-page">
  <section class="hero">
    <div class="hero-content">
      <div class="hero-badge">Your Trusted Online Pharmacy</div>
      <h1 class="hero-title">Medicines Delivered Fast</h1>
      <p class="hero-sub">Browse 13,000+ medicines across 15 categories. Authentic products, competitive prices.</p>
      <a href="/medicines" class="btn btn-primary btn-lg hero-cta">Browse All Medicines</a>
    </div>
  </section>
  <div class="section">
    <div class="section-hd">
      <h2 class="section-title">Shop by Category</h2>
    </div>
    <div class="cat-grid">
      ${catCards}
    </div>
  </div>
  <div class="section">
    <div class="section-hd">
      <h2 class="section-title">Featured Medicines</h2>
      <a href="/medicines" class="view-all">View all</a>
    </div>
    <div class="med-grid">
      ${medCards}
    </div>
  </div>
</div>
${footerHtml()}`;

  const ssrData = { categories: cats, medicines: meds, total };
  return { ssrHtml, ssrData,
    title: 'MediForge — Your Online Pharmacy',
    desc:  'Buy medicines online. 13,000+ medicines across 15 categories. Fast delivery.',
  };
}

/* ── Medicines list SSR ─────────────────────────────────────────────────── */
async function buildListSSR() {
  let cats = [], meds = [], total = 0;
  try {
    const [catData, medData] = await Promise.all([
      nodeFetch('/api/categories/'),
      nodeFetch('/api/medicines/?is_discontinued=false&ordering=-created_at&limit=20'),
    ]);
    cats  = catData.results || catData || [];
    meds  = medData.results || [];
    total = medData.count   || 0;
  } catch (e) {
    console.warn('[SSR] List data fetch failed:', e.message);
  }

  const sidebarItems = cats.map(c => {
    const img = c.image
      ? `<img class="cat-img" src="${esc(c.image)}" alt="${esc(c.name)}" loading="lazy">`
      : `<span style="font-size:1rem">&#x1F48A;</span>`;
    return `<li class="sidebar-item">
      <div class="cat-card">
        <div class="cat-img-wrap">${img}</div>
        <span class="cat-name">${esc(c.name)}</span>
      </div>
    </li>`;
  }).join('\n');

  const medCards = meds.map(medCardHtml).join('\n');

  const ssrHtml = `${navbarHtml('list')}
<div class="list-layout">
  <aside class="cat-sidebar">
    <h3 class="sidebar-title">Categories</h3>
    <ul class="sidebar-list">
      ${sidebarItems}
    </ul>
  </aside>
  <div class="list-main">
    <div class="list-header">
      <h2 class="list-title">All Medicines</h2>
      <span class="result-count">${total} medicines found</span>
    </div>
    <div class="med-grid">
      ${medCards}
    </div>
  </div>
</div>
${footerHtml()}`;

  const ssrData = { categories: cats, medicines: meds, total };
  return { ssrHtml, ssrData,
    title: 'All Medicines — MediForge',
    desc:  'Browse our full catalogue of prescription and OTC medicines.',
  };
}

/* ── Medicine detail SSR ────────────────────────────────────────────────── */
async function buildDetailSSR(slug) {
  let med = null;
  try {
    const data = await nodeFetch(`/api/medicines/?slug=${encodeURIComponent(slug)}`);
    const results = data.results || [];
    if (results.length > 0) med = results[0];
  } catch (e) {
    console.warn('[SSR] Detail data fetch failed:', e.message);
  }

  if (!med) {
    return { ssrHtml: '', ssrData: {}, title: 'MediForge', desc: '' };
  }

  const emoji     = getMedEmoji(med.dosage_form);
  const price     = parseFloat(med.price || 0).toFixed(2);
  const catName   = med.category?.name || '';
  const mfr       = med.manufacturer?.name || '';
  const rxBadge   = med.prescription_required
    ? `<span class="badge badge-rx">Prescription Required</span>` : '';
  const stockBadge = `<span class="badge badge-stock">In Stock</span>`;
  const desc      = esc(med.description || '');

  const ssrHtml = `${navbarHtml('')}
<div class="detail-wrap">
  <div class="breadcrumb">
    <a href="/">Home</a>
    <span>&rsaquo; </span>
    <a href="/medicines">${esc(catName || 'Medicines')}</a>
    <span>&rsaquo; </span>
    <span>${esc(med.name)}</span>
  </div>
  <div class="detail-grid">
    <div class="detail-left">
      <div class="detail-img">${emoji}</div>
      <div class="detail-badges">
        ${rxBadge}
        ${stockBadge}
      </div>
    </div>
    <div class="detail-info">
      <div class="detail-cat">${esc(catName)}</div>
      <h1 class="detail-name">${esc(med.name)}</h1>
      <div class="detail-ingr">${esc(med.primary_ingredient || '')}</div>
      <div class="detail-mfr">By ${esc(mfr)}</div>
      <div class="detail-price-row">
        <span class="detail-price">&#8377;${esc(price)}</span>
        <span class="detail-pack">${esc(med.pack_unit || '')}</span>
      </div>
      <p class="detail-desc">${desc}</p>
      <div class="detail-actions">
        <button class="btn btn-primary btn-lg">Add to Cart</button>
        <a href="/medicines" class="btn btn-outline btn-lg">Back</a>
      </div>
    </div>
  </div>
</div>
${footerHtml()}`;

  const seoDesc = (med.description || med.name + ' ' + catName).slice(0, 155);
  const ssrData = { medicine: med };
  return { ssrHtml, ssrData,
    title: med.name + ' — MediForge',
    desc:  seoDesc,
    ogType: 'product',
  };
}

/* ── Inject SSR into HTML template ─────────────────────────────────────── */
function injectSSR(template, { ssrHtml, ssrData, title, desc, ogType }) {
  /* 1. Replace page title */
  let html = template.replace(/<title>[^<]*<\/title>/, `<title>${esc(title)}</title>`);

  /* 2. Update meta description */
  html = html.replace(
    /(<meta\s+name="description"\s+content=")[^"]*(")/,
    `$1${esc(desc)}$2`
  );

  /* 3. Update og:title */
  html = html.replace(
    /(<meta\s+property="og:title"\s+content=")[^"]*(")/,
    `$1${esc(title)}$2`
  );

  /* 4. Update og:description */
  html = html.replace(
    /(<meta\s+property="og:description"\s+content=")[^"]*(")/,
    `$1${esc(desc)}$2`
  );

  /* 5. Update twitter:title */
  html = html.replace(
    /(<meta\s+name="twitter:title"\s+content=")[^"]*(")/,
    `$1${esc(title)}$2`
  );

  /* 6. Update og:type if provided */
  if (ogType) {
    html = html.replace(
      /(<meta\s+property="og:type"\s+content=")[^"]*(")/,
      `$1${esc(ogType)}$2`
    );
  }

  /* 7. Inject SSR data + clear-patch script into <head> */
  const ssrDataJson = JSON.stringify(ssrData).replace(/<\/script/gi, '<\\/script');
  const headInject = `
<script>
/* SSR data — pre-fetched by SSR server to avoid redundant API calls */
window.__SSR_DATA__ = ${ssrDataJson};

/* Clear SSR content right before Forge mounts (Forge renders fresh, no conflicts) */
(function() {
  var _od = customElements.define.bind(customElements);
  customElements.define = function(name, cls, opts) {
    if (name === 'forge-app') {
      var el = document.getElementById('app');
      if (el) el.innerHTML = '';
    }
    return _od(name, cls, opts);
  };
})();
</script>`;

  html = html.replace('</head>', headInject + '\n</head>');

  /* 8. Inject SSR HTML into <forge-app id="app"> */
  if (ssrHtml) {
    html = html.replace(
      '<forge-app id="app"></forge-app>',
      `<forge-app id="app">\n<!-- SSR: Pre-rendered for view-source, SEO, and no-JS browsers -->\n${ssrHtml}\n</forge-app>`
    );
  }

  return html;
}

/* ── Load and cache the HTML template ──────────────────────────────────── */
function loadTemplate() {
  /* Try base_index.html first (authoritative), fall back to index.html */
  const candidates = ['base_index.html', 'index.html'];
  for (const name of candidates) {
    const p = path.join(DIR, name);
    if (fs.existsSync(p)) return fs.readFileSync(p, 'utf8');
  }
  throw new Error('No index.html or base_index.html found in ' + DIR);
}

/* ── API Proxy ─────────────────────────────────────────────────────────────── */
function proxyApi(req, res) {
  const parsed   = new urlMod.URL(API_BASE);
  const isHttps  = parsed.protocol === 'https:';
  const target   = `${API_BASE}${req.url}`;
  const upUrl    = new urlMod.URL(target);

  const headers = {
    'Accept':          'application/json',
    'Accept-Language': 'en-US,en;q=0.9',
    'Host':            upUrl.host,
  };
  if (API_TOKEN) headers['Authorization'] = 'Bearer ' + API_TOKEN;

  /* Forward the request body (for POST) */
  let bodyChunks = [];
  req.on('data', c => bodyChunks.push(c));
  req.on('end', () => {
    const body = Buffer.concat(bodyChunks);
    if (body.length) headers['Content-Length'] = body.length;
    if (req.headers['content-type']) headers['Content-Type'] = req.headers['content-type'];

    const options = {
      hostname: upUrl.hostname,
      port:     upUrl.port || (isHttps ? 443 : 80),
      path:     upUrl.pathname + upUrl.search,
      method:   req.method,
      headers,
    };

    const transport  = isHttps ? https : http;
    const proxyReq   = transport.request(options, proxyRes => {
      const respHeaders = {
        'Content-Type':                proxyRes.headers['content-type'] || 'application/json',
        'Access-Control-Allow-Origin': '*',
      };
      if (proxyRes.headers['content-encoding'])
        respHeaders['Content-Encoding'] = proxyRes.headers['content-encoding'];
      res.writeHead(proxyRes.statusCode, respHeaders);
      proxyRes.pipe(res);
    });

    proxyReq.on('error', err => {
      console.error(`[proxy] ${req.url} FAILED:`, err.message);
      if (!res.headersSent) {
        res.writeHead(502, { 'Content-Type': 'text/plain' });
        res.end(`Proxy error: ${err.message}`);
      }
    });

    if (body.length) proxyReq.write(body);
    proxyReq.end();
  });
}

/* ── Static file server ─────────────────────────────────────────────────── */
function serveFile(filePath, res) {
  try {
    const data = fs.readFileSync(filePath);
    const ext  = path.extname(filePath).toLowerCase();
    res.writeHead(200, {
      'Content-Type':  MIME[ext] || 'application/octet-stream',
      'Cache-Control': ext === '.html' ? 'no-cache' : 'public, max-age=300',
    });
    res.end(data);
  } catch {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not found: ' + filePath);
  }
}

/* ── SSR route handler ──────────────────────────────────────────────────── */
async function serveSSR(reqPath, res) {
  let ssrResult;
  try {
    if (reqPath === '/' || reqPath === '/home') {
      ssrResult = await buildHomeSSR();
    } else if (reqPath === '/medicines') {
      ssrResult = await buildListSSR();
    } else {
      /* /medicine/:slug */
      const m = reqPath.match(/^\/medicine\/(.+)$/);
      if (m) {
        ssrResult = await buildDetailSSR(m[1]);
      } else {
        /* /cart and any other SPA route — serve template without SSR data */
        ssrResult = { ssrHtml: '', ssrData: {}, title: 'MediForge', desc: '' };
      }
    }
  } catch (e) {
    console.error('[SSR] Build failed:', e.message);
    ssrResult = { ssrHtml: '', ssrData: {}, title: 'MediForge', desc: '' };
  }

  let html;
  try {
    const template = loadTemplate();
    html = injectSSR(template, ssrResult);
  } catch (e) {
    console.error('[SSR] Template inject failed:', e.message);
    html = '<html><body>Server error</body></html>';
  }

  res.writeHead(200, {
    'Content-Type':  'text/html; charset=utf-8',
    'Cache-Control': 'no-cache',
  });
  res.end(html);
}

/* ── Request handler ─────────────────────────────────────────────────────── */
const server = http.createServer((req, res) => {
  /* CORS preflight */
  if (req.method === 'OPTIONS') {
    res.writeHead(204, {
      'Access-Control-Allow-Origin':  '*',
      'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
      'Access-Control-Allow-Headers': 'Authorization, Content-Type, Accept',
    });
    res.end();
    return;
  }

  const reqPath = (req.url || '/').split('?')[0];

  /* Proxy /api/* → Django */
  if (reqPath.startsWith('/api/')) {
    console.log(`[proxy] ${req.method} ${req.url}`);
    proxyApi(req, res);
    return;
  }

  /* Static assets (have extension) */
  const ext = path.extname(reqPath);
  if (ext) {
    const filePath = path.join(DIR, reqPath);
    serveFile(filePath, res);
    return;
  }

  /* SPA routes — SSR */
  console.log(`[ssr]   GET ${reqPath}`);
  serveSSR(reqPath, res).catch(e => {
    console.error('[ssr] Unhandled error:', e);
    res.writeHead(500, { 'Content-Type': 'text/plain' });
    res.end('Internal server error');
  });
});

/* ── Start ─────────────────────────────────────────────────────────────────── */
server.listen(PORT, () => {
  console.log(`\n  \x1b[32mMediForge SSR Server\x1b[0m`);
  console.log(`  \x1b[36mLocal:\x1b[0m    http://localhost:${PORT}`);
  console.log(`  \x1b[36mAPI:\x1b[0m      ${API_BASE}`);
  console.log(`  \x1b[36mAuth:\x1b[0m     ${API_TOKEN ? 'Bearer token set \u2713' : '\x1b[33mNo token set (cart/auth endpoints may fail)\x1b[0m'}`);
  console.log(`\n  SSR Routes:`);
  console.log(`    http://localhost:${PORT}/             → Home (categories + medicines injected)`);
  console.log(`    http://localhost:${PORT}/medicines    → All Medicines (medicines injected)`);
  console.log(`    http://localhost:${PORT}/medicine/<slug> → Detail (full medicine data injected)`);
  console.log(`    http://localhost:${PORT}/cart         → Cart (no SSR, auth required)`);
  console.log(`\n  How it works:`);
  console.log(`    1. Fetches data from Django API at request time`);
  console.log(`    2. Injects HTML into <forge-app> — visible in view-source`);
  console.log(`    3. Forge JS takes over seamlessly after load`);
  console.log(`\n  Press Ctrl+C to stop\n`);
});
