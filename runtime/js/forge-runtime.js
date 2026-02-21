/**
 * Forge Runtime - Browser Bootstrap
 * ~2KB minified. Zero dependencies.
 *
 * This script:
 *   1. Provides the WASM import object (DOM bridge functions)
 *   2. Manages the DOM node ID table
 *   3. Routes browser events into WASM
 *   4. Schedules re-renders via requestAnimationFrame
 *   5. Exports ForgeRuntime and ForgeComponent base class
 */

'use strict';

/* ─── DOM Node Table ──────────────────────────────────────────────────────── */
// Maps integer IDs (used inside WASM) to live DOM nodes

const _nodeTable   = new Map();  // id → Node
const _nodeIds     = new WeakMap(); // Node → id
let   _nextNodeId  = 1;

function _nodeRegister(node) {
  if (_nodeIds.has(node)) return _nodeIds.get(node);
  const id = _nextNodeId++;
  _nodeTable.set(id, node);
  _nodeIds.set(node, id);
  node.__forgeId = id;
  return id;
}

function _nodeGet(id) { return _nodeTable.get(id) || null; }

function _nodeRemove(id) {
  const node = _nodeTable.get(id);
  if (node) { _nodeIds.delete(node); _nodeTable.delete(id); }
}

/* ─── WASM Memory Helpers ─────────────────────────────────────────────────── */

let _wasmMemory = null;   // set when first WASM module loads

function _mem() { return _wasmMemory.buffer; }

function _readStr(ptr, len) {
  return new TextDecoder().decode(new Uint8Array(_mem(), ptr, len));
}

function _writeStr(str) {
  const enc  = new TextEncoder().encode(str);
  const mem  = new Uint8Array(_mem());
  // Allocate in a scratch area — real impl uses WASM arena
  const ptr  = 65536; // fixed scratch for now
  mem.set(enc, ptr);
  return { ptr, len: enc.length };
}

function _readVal(ptr) {
  const view = new DataView(_mem());
  const kind = view.getInt32(ptr,     true);
  const lo   = view.getInt32(ptr + 4, true);
  const hi   = view.getInt32(ptr + 8, true);
  switch (kind) {
    case 0: return null;
    case 1: return lo | (hi * 0x100000000); // i64 (approx)
    case 2: return view.getFloat64(ptr + 4, true);
    case 3: return !!lo;
    case 4: return _readStr(lo, hi);
    default: return null;
  }
}

/* ─── Event Table ─────────────────────────────────────────────────────────── */

const _eventListeners = new Map(); // `${nodeId}:${eventName}` → { cb, ctx }

/* FNV-1a hash (matches forge_fnv1a in C) */
function _fnv1a(str) {
  let h = 2166136261;
  for (let i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = (h * 16777619) >>> 0;
  }
  return h;
}

/* ─── WASM Import Object ──────────────────────────────────────────────────── */

function _buildImports(componentName, wasmExports) {
  return {
    env: {
      /* ── DOM Creation ── */
      forge_dom_create(parentId, tagPtr, tagLen) {
        const parent = _nodeGet(parentId) || document.body;
        const tag    = _readStr(tagPtr, tagLen);
        const el     = document.createElement(tag);
        parent.appendChild(el);
        return _nodeRegister(el);
      },

      forge_dom_text(parentId, textPtr, textLen) {
        const parent = _nodeGet(parentId);
        if (!parent) return;
        const text = _readStr(textPtr, textLen);
        const tn   = document.createTextNode(text);
        parent.appendChild(tn);
        _nodeRegister(tn);
      },

      forge_dom_expr(parentId, fnPtr, ctxPtr) {
        const parent = _nodeGet(parentId);
        if (!parent) return;
        const tn = document.createTextNode('');
        parent.appendChild(tn);
        const id = _nodeRegister(tn);
        // Store for reactive updates
        tn.__forgeExprFn  = fnPtr;
        tn.__forgeExprCtx = ctxPtr;
        tn.__forgeExports = wasmExports;
        // Initial evaluation
        _evalExpr(tn, wasmExports, fnPtr, ctxPtr);
      },

      forge_dom_get(elId) {
        const el = document.querySelector(`[data-forge-id="${elId}"]`) ||
                   _nodeGet(elId);
        if (el && !_nodeIds.has(el)) _nodeRegister(el);
        return el ? _nodeIds.get(el) : 0;
      },

      forge_dom_create_component(parentId, namePtr, nameLen) {
        const parent = _nodeGet(parentId);
        const name   = _readStr(namePtr, nameLen);
        const tag    = 'forge-' + name.replace(/([A-Z])/g, (_, c, i) =>
                         (i ? '-' : '') + c.toLowerCase());
        const el     = document.createElement(tag);
        if (parent) parent.appendChild(el);
        return _nodeRegister(el);
      },

      /* ── Attributes ── */
      forge_dom_set_attr(nodeId, namePtr, nameLen, valPtr, valLen) {
        const el  = _nodeGet(nodeId);
        if (!el || !el.setAttribute) return;
        const name = _readStr(namePtr, nameLen);
        const val  = _readStr(valPtr,  valLen);
        el.setAttribute(name, val);
      },

      forge_dom_set_attr_expr(nodeId, namePtr, nameLen, fnPtr, ctxPtr) {
        const el   = _nodeGet(nodeId);
        if (!el || !el.setAttribute) return;
        const name = _readStr(namePtr, nameLen);
        el.__forgeAttrExprs = el.__forgeAttrExprs || {};
        el.__forgeAttrExprs[name] = { fnPtr, ctxPtr, exports: wasmExports };
        // Initial eval
        try {
          const result = wasmExports.__indirect_function_table.get(fnPtr)(ctxPtr);
          const val    = _valToString(result);
          el.setAttribute(name, val);
        } catch(e) { /* fn ptr may be 0 */ }
      },

      forge_dom_set_prop(nodeId, namePtr, nameLen, valKind, valLo, valHi) {
        const el   = _nodeGet(nodeId);
        const name = _readStr(namePtr, nameLen);
        if (el) el[name] = _rawToVal(valKind, valLo, valHi);
      },

      forge_dom_set_prop_str(nodeId, namePtr, nameLen, valPtr, valLen) {
        const el   = _nodeGet(nodeId);
        const name = _readStr(namePtr, nameLen);
        const val  = _readStr(valPtr,  valLen);
        if (el) el[name] = val;
      },

      /* ── Styles ── */
      forge_dom_set_style(nodeId, propPtr, propLen, fnPtr, staticPtr, staticLen) {
        const el   = _nodeGet(nodeId);
        if (!el || !el.style) return;
        const prop = _readStr(propPtr,   propLen);
        if (fnPtr) {
          el.__forgeStyleExprs = el.__forgeStyleExprs || {};
          el.__forgeStyleExprs[prop] = { fnPtr, exports: wasmExports };
        } else {
          const val = _readStr(staticPtr, staticLen);
          el.style[prop] = val;
        }
      },

      forge_dom_inject_css(namePtr, nameLen, cssPtr, cssLen) {
        const name = _readStr(namePtr, nameLen);
        const css  = _readStr(cssPtr,  cssLen);
        const id   = `forge-style-${name}`;
        if (document.getElementById(id)) return;
        const style = document.createElement('style');
        style.id = id;
        style.textContent = css;
        document.head.appendChild(style);
      },

      /* ── Events ── */
      forge_dom_on(nodeId, evtPtr, evtLen, cbPtr, ctxPtr) {
        const el  = _nodeGet(nodeId);
        if (!el) return;
        const evt = _readStr(evtPtr, evtLen);
        const key = `${nodeId}:${evt}`;

        // Remove old listener if any
        if (_eventListeners.has(key)) {
          const old = _eventListeners.get(key);
          el.removeEventListener(evt, old.handler);
        }

        const handler = (browserEvent) => {
          // Build forge_event_t in WASM memory
          const evBuf = new Uint8Array(_mem(), 0, 24); // scratch
          const view  = new DataView(evBuf.buffer, 0, 24);
          view.setUint32(0,  _fnv1a(evt),  true); // type_hash
          view.setUint32(4,  nodeId,        true); // target_id
          view.setInt32 (8,  browserEvent.which || 0, true);
          view.setFloat32(12, browserEvent.clientX || 0, true);
          view.setFloat32(16, browserEvent.clientY || 0, true);
          const flags =
            (browserEvent.shiftKey ? 1 : 0) |
            (browserEvent.ctrlKey  ? 2 : 0) |
            (browserEvent.altKey   ? 4 : 0) |
            (browserEvent.metaKey  ? 8 : 0);
          view.setUint32(20, flags, true);

          try {
            wasmExports.__indirect_function_table.get(cbPtr)(0, ctxPtr);
          } catch (e) {
            console.error('[Forge] Event handler error:', e);
          }
        };

        el.addEventListener(evt, handler);
        _eventListeners.set(key, { handler, cbPtr, ctxPtr });
      },

      forge_dom_off(nodeId, evtPtr, evtLen) {
        const el  = _nodeGet(nodeId);
        if (!el) return;
        const evt = _readStr(evtPtr, evtLen);
        const key = `${nodeId}:${evt}`;
        if (_eventListeners.has(key)) {
          el.removeEventListener(evt, _eventListeners.get(key).handler);
          _eventListeners.delete(key);
        }
      },

      /* ── DOM Mutation ── */
      forge_dom_remove(nodeId) {
        const el = _nodeGet(nodeId);
        if (el && el.parentNode) el.parentNode.removeChild(el);
        _nodeRemove(nodeId);
      },

      forge_dom_clear(nodeId) {
        const el = _nodeGet(nodeId);
        if (el) el.innerHTML = '';
      },

      /* ── RAF / Scheduling ── */
      js_schedule_raf() {
        requestAnimationFrame(() => {
          if (wasmExports && wasmExports.forge_raf_callback) {
            wasmExports.forge_raf_callback();
          }
        });
      },

      /* ── Logging ── */
      js_console_log(ptr, len) {
        console.log('[Forge]', _readStr(ptr, len));
      },

      js_console_log_int(labelPtr, labelLen, valLo, valHi) {
        const label = _readStr(labelPtr, labelLen);
        const val   = valLo + valHi * 0x100000000;
        console.log('[Forge]', label, val);
      },

      js_trap(msgPtr, msgLen) {
        const msg = _readStr(msgPtr, msgLen);
        throw new Error(`[Forge TRAP] ${msg}`);
      },
    }
  };
}

/* ─── Value Helpers ───────────────────────────────────────────────────────── */

function _evalExpr(node, exports, fnPtr, ctxPtr) {
  try {
    const fn  = exports.__indirect_function_table?.get(fnPtr);
    if (!fn) return;
    const result = fn(ctxPtr);
    node.textContent = _valToString(result);
  } catch(e) { /* ignore */ }
}

function _valToString(val) {
  if (val === null || val === undefined) return '';
  if (typeof val === 'object' && 'v' in val) {
    switch (val.kind) {
      case 1: return String(val.v.i);
      case 2: return String(val.v.f.toFixed(6).replace(/\.?0+$/, ''));
      case 3: return val.v.b ? 'true' : 'false';
      case 4: return _readStr(val.v.str_ptr, 256); // read until null
      default: return '';
    }
  }
  return String(val);
}

function _rawToVal(kind, lo, hi) {
  switch (kind) {
    case 1: return lo;
    case 2: return lo; // float passed as raw bits — simplification
    case 3: return !!lo;
    case 4: return _readStr(lo, hi);
    default: return null;
  }
}

/* ─── Component Registry ──────────────────────────────────────────────────── */

const _componentExports = new Map();

/* ─── ForgeComponent Base Class ───────────────────────────────────────────── */

class ForgeComponent extends HTMLElement {
  static observedProps = [];
  static wasmReady     = Promise.resolve();
  static tag           = 'forge-component';

  constructor() {
    super();
    this._props    = {};
    this._mounted  = false;
    this.__forgeId = _nodeRegister(this);
  }

  connectedCallback() {
    this.constructor.wasmReady.then(() => {
      this._syncPropsFromAttrs();
      this.mount(this);
      this._mounted = true;
    });
  }

  disconnectedCallback() {
    if (this._mounted) {
      this.unmount(this);
      this._mounted = false;
      _nodeRemove(this.__forgeId);
    }
  }

  attributeChangedCallback(name, _, newValue) {
    this._props[name] = newValue;
    if (this._mounted) this.update(this, this._props);
  }

  static get observedAttributes() { return this.observedProps; }

  _syncPropsFromAttrs() {
    for (const prop of this.constructor.observedProps) {
      const attr = this.getAttribute(prop);
      if (attr !== null) this._props[prop] = attr;
    }
  }

  /* Subclasses override these */
  mount(el)              { /* override in generated class */ }
  update(el, props)      { /* override in generated class */ }
  dispatch(el, event)    { /* override in generated class */ }
  unmount(el)            { /* override in generated class */ }
}

/* ─── ForgeRuntime Namespace ──────────────────────────────────────────────── */

const ForgeRuntime = {
  version: '0.1.0',

  wasmImports(componentName) {
    return _buildImports(componentName, _componentExports.get(componentName) || {});
  },

  registerExports(componentName, exports) {
    _componentExports.set(componentName, exports);
    if (!_wasmMemory && exports.memory) {
      _wasmMemory = exports.memory;
    }
    // Initialize the runtime
    if (exports.forge_runtime_init) exports.forge_runtime_init();
  },

  serializeProps(props) {
    /* Pack props into WASM memory as a binary blob.
     * For now: JSON encode and write as a string (fast-path: C struct copy).
     * Production: use the compiler-generated struct layout metadata. */
    const json = JSON.stringify(props || {});
    const enc  = new TextEncoder().encode(json);
    const buf  = new Uint8Array(_wasmMemory?.buffer || new ArrayBuffer(4096));
    const ptr  = 0x10000; // scratch area offset
    buf.set(enc, ptr);
    return { ptr, len: enc.length };
  },

  serializeEvent(event) {
    const buf  = _wasmMemory
      ? new DataView(_wasmMemory.buffer, 0, 32)
      : new DataView(new ArrayBuffer(32));
    buf.setUint32(0, _fnv1a(event.type || ''), true);
    buf.setUint32(4, event.target?.__forgeId || 0, true);
    return { ptr: 0 };
  },

  /* Get live DOM node from WASM node ID */
  getNode: _nodeGet,
  registerNode: _nodeRegister,
};

/* ─── Exports ─────────────────────────────────────────────────────────────── */

export { ForgeRuntime, ForgeComponent };
export default ForgeRuntime;
