#!/usr/bin/env node
'use strict';
/**
 * MediForge Dev Server — examples/06-medicine-shop/dev-server.js
 *
 * Serves static files AND proxies /api/* to the Django backend.
 * This avoids CORS issues since everything is same-origin.
 *
 * Usage:
 *   node dev-server.js [--port=3002] [--api=http://localhost:8000] [--token=Bearer...]
 *
 * Or set env vars:
 *   MEDIFORGE_API=http://localhost:8000 MEDIFORGE_TOKEN=eyJ... node dev-server.js
 */

const http = require('http');
const fs   = require('fs');
const path = require('path');
const url  = require('url');

/* ── Config ────────────────────────────────────────────────────────────────── */
const DIR   = __dirname;
const PORT  = (() => {
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

/* ── API Proxy ─────────────────────────────────────────────────────────────── */
function proxyApi(req, res) {
  const parsed   = new url.URL(API_BASE);
  const isHttps  = parsed.protocol === 'https:';
  const target   = `${API_BASE}${req.url}`;   /* full upstream URL */
  const upUrl    = new url.URL(target);

  const headers = {
    'Accept':          'application/json',
    'Accept-Language': 'en-US,en;q=0.9',
    'Host':            upUrl.host,
  };
  if (API_TOKEN) headers['Authorization'] = 'Bearer ' + API_TOKEN;

  const options = {
    hostname: upUrl.hostname,
    port:     upUrl.port || (isHttps ? 443 : 80),
    path:     upUrl.pathname + upUrl.search,
    method:   req.method,
    headers,
  };

  const transport = isHttps ? require('https') : http;
  const proxyReq  = transport.request(options, proxyRes => {
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
    console.error(`[proxy] ${req.url} → ${target} FAILED:`, err.message);
    if (!res.headersSent) {
      res.writeHead(502, { 'Content-Type': 'text/plain' });
      res.end(`Proxy error: could not reach ${API_BASE}\n${err.message}`);
    }
  });

  req.pipe(proxyReq);
}

/* ── Static file server ────────────────────────────────────────────────────── */
function serveFile(filePath, res) {
  try {
    const data = fs.readFileSync(filePath);
    const ext  = path.extname(filePath).toLowerCase();
    res.writeHead(200, {
      'Content-Type':  MIME[ext] || 'application/octet-stream',
      'Cache-Control': ext === '.html' ? 'no-cache' : 'public, max-age=60',
    });
    res.end(data);
  } catch {
    /* File not found — serve index.html for SPA routes */
    try {
      const html = fs.readFileSync(path.join(DIR, 'index.html'));
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-cache' });
      res.end(html);
    } catch {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('Not found');
    }
  }
}

/* ── Request handler ────────────────────────────────────────────────────────── */
const server = http.createServer((req, res) => {
  /* Handle CORS preflight */
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

  /* Proxy all /api/* requests to Django backend */
  if (reqPath.startsWith('/api/')) {
    console.log(`[proxy] ${req.method} ${req.url} → ${API_BASE}${req.url}`);
    proxyApi(req, res);
    return;
  }

  /* Static assets with extension */
  const ext = path.extname(reqPath);
  if (ext) {
    const filePath = path.join(DIR, reqPath);
    serveFile(filePath, res);
    return;
  }

  /* SPA routes (/, /medicines, /medicine/:id, /cart) → index.html */
  serveFile(path.join(DIR, 'index.html'), res);
});

/* ── Start ─────────────────────────────────────────────────────────────────── */
server.listen(PORT, () => {
  console.log(`\n  \x1b[32mMediForge Dev Server\x1b[0m`);
  console.log(`  \x1b[36mLocal:\x1b[0m    http://localhost:${PORT}`);
  console.log(`  \x1b[36mAPI:\x1b[0m      ${API_BASE} (proxied at /api/)`);
  console.log(`  \x1b[36mAuth:\x1b[0m     ${API_TOKEN ? 'Bearer token set ✓' : '\x1b[31mNo token — set --token=<jwt> or MEDIFORGE_TOKEN\x1b[0m'}`);
  console.log(`\n  Routes:`);
  console.log(`    http://localhost:${PORT}/             → Home`);
  console.log(`    http://localhost:${PORT}/medicines    → All Medicines`);
  console.log(`    http://localhost:${PORT}/medicine/1   → Medicine Detail`);
  console.log(`    http://localhost:${PORT}/cart         → Cart`);
  console.log(`\n  Press Ctrl+C to stop\n`);
});
