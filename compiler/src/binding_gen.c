/*
 * Forge Framework - JS Binding Generator
 *
 * Two output modes:
 *   1. WASM mode (default): thin JS loader that instantiates the .wasm module
 *   2. No-WASM mode (--no-wasm): standalone JS that creates DOM directly from
 *      the component AST — no WASM, no Clang, fully self-contained.
 */

#include "binding_gen.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void lower(char *dst, const char *src) {
  while (*src) {
    *dst++ = (char)tolower((unsigned char)*src++);
  }
  *dst = '\0';
}

static void kebab(char *dst, const char *src) {
  /* "MyButton" → "my-button" */
  int first = 1;
  while (*src) {
    char c = *src++;
    if (isupper((unsigned char)c) && !first)
      *dst++ = '-';
    *dst++ = (char)tolower((unsigned char)c);
    first = 0;
  }
  *dst = '\0';
}

static void emit_type_ts(const TypeRef *t, FILE *out) {
  if (!t) {
    fprintf(out, "any");
    return;
  }
  switch (t->kind) {
  case TY_INT:
  case TY_LONG:
  case TY_SHORT:
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_UNSIGNED:
    fprintf(out, "number");
    break;
  case TY_CHAR:
    fprintf(out, "string");
    break;
  case TY_BOOL:
    fprintf(out, "boolean");
    break;
  case TY_VOID:
    fprintf(out, "void");
    break;
  case TY_PTR:
    if (t->inner && t->inner->kind == TY_CHAR)
      fprintf(out, "string");
    else
      fprintf(out, "number");
    break;
  case TY_FN_PTR:
    fprintf(out, "(...args: any[]) => any");
    break;
  default:
    fprintf(out, "any");
    break;
  }
}

/* ─── Escape a string for JS output ──────────────────────────────────────────
 */

static void emit_js_str(const char *s, FILE *out) {
  if (!s) {
    fprintf(out, "''");
    return;
  }
  fprintf(out, "'");
  for (const char *p = s; *p; p++) {
    if (*p == '\'')
      fprintf(out, "\\'");
    else if (*p == '\\')
      fprintf(out, "\\\\");
    else if (*p == '\n')
      fprintf(out, "\\n");
    else if (*p == '\r')
      fprintf(out, "\\r");
    else
      fputc(*p, out);
  }
  fprintf(out, "'");
}

/* ─── Helper: translate a C expression string to JS ─────────────────────────
 */

static void emit_expr_js(const char *expr, FILE *out) {
  const char *p = expr;
  while (*p) {
    if (strncmp(p, "state.", 6) == 0) {
      fprintf(out, "this._state.");
      p += 6;
    } else if (strncmp(p, "props.", 6) == 0) {
      fprintf(out, "this._props.");
      p += 6;
    } else if (strncmp(p, "computed.", 9) == 0) {
      fprintf(out, "this._getComputed().");
      p += 9;
    } else {
      fputc(*p, out);
      p++;
    }
  }
}

/* ─── No-WASM: Emit DOM creation JS for an HTML node ────────────────────────
 */

static int _nw_id = 0;

static void emit_nw_html(const HtmlNode *n, const char *parent_var,
                         const ComponentNode *comp, FILE *out) {
  if (!n)
    return;
  char lname[256];
  lower(lname, comp->name);

  switch (n->kind) {
  case HTML_TEXT:
    if (n->text && n->text[0]) {
      fprintf(out, "      %s.appendChild(document.createTextNode(", parent_var);
      emit_js_str(n->text, out);
      fprintf(out, "));\n");
    }
    break;

  case HTML_EXPR:
    /* Expression node: create text node whose content is the C expression
     * mapped to JS.  e.g. state.count → this._state.count */
    if (n->text) {
      int id = _nw_id++;
      fprintf(out, "      { const __tn%d = document.createTextNode('');\n", id);
      /* Build a reactive updater */
      fprintf(out, "        __tn%d.__forgeUpdate = () => {\n", id);
      fprintf(out, "          __tn%d.textContent = String(", id);
      emit_expr_js(n->text, out);
      fprintf(out, ");\n");
      fprintf(out, "        };\n");
      fprintf(out, "        __tn%d.__forgeUpdate();\n", id);
      fprintf(out, "        this._exprNodes.push(__tn%d);\n", id);
      fprintf(out, "        %s.appendChild(__tn%d);\n", parent_var, id);
      fprintf(out, "      }\n");
    }
    break;

  case HTML_COMPONENT: {
    int id = _nw_id++;
    char ctag[256];
    kebab(ctag, n->tag ? n->tag : "div");
    fprintf(out, "      { const __cc%d = document.createElement('forge-%s');\n",
            id, ctag);
    /* Set props as attributes / properties */
    for (int i = 0; i < n->attr_count; i++) {
      const char *aname = n->attrs[i].name;
      const char *aval = n->attrs[i].value ? n->attrs[i].value : "";
      if (n->attrs[i].is_expr) {
        fprintf(out, "        __cc%d['%s'] = %s;\n", id, aname, aval);
      } else {
        fprintf(out, "        __cc%d.setAttribute('%s', ", id, aname);
        emit_js_str(aval, out);
        fprintf(out, ");\n");
      }
    }
    fprintf(out, "        %s.appendChild(__cc%d);\n", parent_var, id);
    fprintf(out, "      }\n");
    break;
  }

  case HTML_ELEMENT: {
    int id = _nw_id++;
    fprintf(out, "      const __e%d = document.createElement('%s');\n", id,
            n->tag ? n->tag : "div");

    /* Attributes */
    for (int i = 0; i < n->attr_count; i++) {
      const char *aname = n->attrs[i].name;
      const char *aval = n->attrs[i].value ? n->attrs[i].value : "";

      /* Event binding: onclick={@handler} */
      if (strncmp(aname, "on", 2) == 0 && islower((unsigned char)aname[2])) {
        /* aval is the handler reference like @set_filter_all */
        const char *hname = aval;
        if (*hname == '@')
          hname++;
        fprintf(out, "      __e%d.addEventListener('%s', (e) => {\n", id,
                aname + 2);
        fprintf(out, "        this._handle_%s(e);\n", hname);
        fprintf(out, "        this._refresh();\n");
        fprintf(out, "      });\n");
      } else if (n->attrs[i].is_expr) {
        /* Dynamic attribute: class={state.x ? "a" : "b"} */
        int aid = _nw_id++;
        fprintf(out, "      { const __ae%d = () => {\n", aid);
        fprintf(out, "          __e%d.setAttribute('%s', String(", id, aname);
        emit_expr_js(aval, out);
        fprintf(out, "));\n");
        fprintf(out, "        };\n");
        fprintf(out, "        __ae%d();\n", aid);
        fprintf(out, "        this._attrUpdaters.push(__ae%d);\n", aid);
        fprintf(out, "      }\n");
      } else {
        fprintf(out, "      __e%d.setAttribute('%s', ", id, aname);
        emit_js_str(aval, out);
        fprintf(out, ");\n");
      }
    }

    /* Recurse children — merge consecutive TEXT nodes into single textContent
     */
    char child_var[64];
    snprintf(child_var, sizeof(child_var), "__e%d", id);
    for (int i = 0; i < n->child_count; i++) {
      /* Merge consecutive HTML_TEXT children into one text node */
      if (n->children[i].kind == HTML_TEXT && n->children[i].text) {
        /* Count how many consecutive text nodes follow */
        int j = i;
        while (j < n->child_count && n->children[j].kind == HTML_TEXT &&
               n->children[j].text)
          j++;
        if (j > i) {
          /* Build merged string: word1 + " " + word2 + " " + ... */
          fprintf(out, "      %s.appendChild(document.createTextNode(",
                  child_var);
          int first = 1;
          for (int k = i; k < j; k++) {
            const char *t = n->children[k].text;
            /* Skip whitespace-only text nodes */
            int all_ws = 1;
            for (const char *c = t; *c; c++) {
              if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') {
                all_ws = 0;
                break;
              }
            }
            if (all_ws)
              continue;
            /* Trim leading/trailing whitespace from the text */
            const char *ts = t;
            while (*ts == ' ' || *ts == '\t' || *ts == '\n' || *ts == '\r')
              ts++;
            const char *te = t + strlen(t) - 1;
            while (te > ts &&
                   (*te == ' ' || *te == '\t' || *te == '\n' || *te == '\r'))
              te--;
            size_t tlen = (size_t)(te - ts + 1);
            if (tlen == 0)
              continue;
            if (!first)
              fprintf(out, " + ' ' + ");
            fprintf(out, "'");
            for (size_t ci = 0; ci < tlen; ci++) {
              char ch = ts[ci];
              if (ch == '\'')
                fprintf(out, "\\'");
              else if (ch == '\\')
                fprintf(out, "\\\\");
              else if (ch == '\n')
                fprintf(out, " ");
              else
                fputc(ch, out);
            }
            fprintf(out, "'");
            first = 0;
          }
          if (first)
            fprintf(out, "''"); /* all whitespace */
          fprintf(out, "));\n");
          i = j - 1; /* skip merged nodes */
          continue;
        }
      }
      emit_nw_html(&n->children[i], child_var, comp, out);
    }

    fprintf(out, "      %s.appendChild(__e%d);\n", parent_var, id);
    break;
  }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 *  NO-WASM BINDING: Self-contained JS component with full DOM rendering     *
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int emit_nowasm_component(const ComponentNode *c,
                                 const BindingOptions *opts, FILE *out) {
  char lname[256], tag[256];
  lower(lname, c->name);
  kebab(tag, c->name);

  /* File header */
  fprintf(out,
          "/**\n"
          " * AUTO-GENERATED by Forge Compiler (no-wasm mode)\n"
          " * Component: %s\n"
          " * Pure JavaScript renderer — no WASM required\n"
          " * DO NOT EDIT — run `forge compile %s.cx` to regenerate\n"
          " */\n\n",
          c->name, c->name);

  /* Start class */
  fprintf(out, "class %s extends HTMLElement {\n", c->name);
  fprintf(out, "  static tag = 'forge-%s';\n\n", tag);

  /* Constructor */
  fprintf(out, "  constructor() {\n");
  fprintf(out, "    super();\n");
  fprintf(out, "    this._props = {};\n");
  fprintf(out, "    this._state = {};\n");
  fprintf(out, "    this._exprNodes = [];\n");
  fprintf(out, "    this._attrUpdaters = [];\n");
  fprintf(out, "    this._mounted = false;\n");
  fprintf(out, "  }\n\n");

  /* Observed attributes */
  fprintf(out, "  static get observedAttributes() {\n");
  fprintf(out, "    return [");
  for (int i = 0; i < c->prop_count; i++) {
    fprintf(out, "%s'%s'", i ? ", " : "", c->props[i].name);
  }
  fprintf(out, "];\n");
  fprintf(out, "  }\n\n");

  /* State initializer */
  fprintf(out, "  _initState() {\n");
  fprintf(out, "    this._state = {\n");
  for (int i = 0; i < c->state_count; i++) {
    fprintf(out, "      %s: %s,\n", c->state[i].name,
            c->state[i].init_expr ? c->state[i].init_expr : "0");
  }
  fprintf(out, "    };\n");
  fprintf(out, "  }\n\n");

  /* Computed properties — _getComputed() returns an object with derived values
   */
  if (c->computed_count > 0) {
    fprintf(out, "  _getComputed() {\n");
    fprintf(out, "    const result = {};\n");
    for (int i = 0; i < c->computed_count; i++) {
      const char *name = c->computed[i].field.name;
      const char *expr = c->computed[i].expression;
      fprintf(out, "    result['%s'] = ", name);

      if (expr) {
        /* Skip leading whitespace in raw expression */
        while (*expr == ' ' || *expr == '\t' || *expr == '\n' || *expr == '\r')
          expr++;
        /* Translate forge_sprintf("...", args) → JS template literal or string
         * concat */
        if (strncmp(expr, "forge_sprintf(", 14) == 0) {
          /* Parse: forge_sprintf("$%d", props.price) */
          const char *p = expr + 14; /* skip 'forge_sprintf(' */
          /* skip whitespace */
          while (*p == ' ')
            p++;
          if (*p == '"') {
            p++; /* skip opening " */
            /* Read format string */
            char fmt[256];
            int fi = 0;
            while (*p && *p != '"' && fi < 254) {
              if (*p == '\\' && *(p + 1)) {
                fmt[fi++] = *(p + 1);
                p += 2;
              } else {
                fmt[fi++] = *p;
                p++;
              }
            }
            fmt[fi] = '\0';
            if (*p == '"')
              p++;
            /* skip comma and whitespace to get args */
            while (*p == ',' || *p == ' ')
              p++;
            /* Remaining is the argument(s) before closing ')' */
            char arg[256];
            int ai = 0;
            int depth = 1;
            while (*p && depth > 0 && ai < 254) {
              if (*p == '(')
                depth++;
              else if (*p == ')') {
                depth--;
                if (depth == 0)
                  break;
              }
              arg[ai++] = *p;
              p++;
            }
            arg[ai] = '\0';
            /* trim trailing whitespace */
            while (ai > 0 && (arg[ai - 1] == ' ' || arg[ai - 1] == '\t'))
              arg[--ai] = '\0';

            /* Build JS string: replace %d, %s etc with template interpolation
             */
            fprintf(out, "(() => { const __v = ");
            emit_expr_js(arg, out);
            fprintf(out, "; return `");
            for (const char *f = fmt; *f; f++) {
              if (*f == '%' &&
                  (*(f + 1) == 'd' || *(f + 1) == 's' || *(f + 1) == 'f')) {
                fprintf(out, "${__v}");
                f++;
              } else if (*f == '`') {
                fprintf(out, "\\`");
              } else {
                fputc(*f, out);
              }
            }
            fprintf(out, "`; })()");
          } else {
            /* Fallback: just emit as-is */
            emit_expr_js(expr, out);
          }
        } else {
          /* Regular expression */
          emit_expr_js(expr, out);
        }
      } else {
        fprintf(out, "null");
      }
      fprintf(out, ";\n");
    }
    fprintf(out, "    return result;\n");
    fprintf(out, "  }\n\n");
  } else {
    fprintf(out, "  _getComputed() { return {}; }\n\n");
  }

  /* Event handlers */
  for (int i = 0; i < c->handler_count; i++) {
    fprintf(out, "  _handle_%s(event) {\n", c->handlers[i].event_name);
    fprintf(out, "    const state = this._state;\n");
    fprintf(out, "    const props = this._props;\n");
    fprintf(out, "    (function() {\n");
    /* Emit handler body, translating state.X → state.X (works as-is with local
     * alias) */
    if (c->handlers[i].body) {
      fprintf(out, "      %s\n", c->handlers[i].body);
    }
    fprintf(out, "    }).call(this);\n");
    fprintf(out, "  }\n\n");
  }

  /* Refresh: re-evaluate all reactive expressions and dynamic attributes */
  fprintf(out, "  _refresh() {\n");
  fprintf(out, "    for (const fn of this._exprNodes) { if (fn.__forgeUpdate) "
               "fn.__forgeUpdate(); }\n");
  fprintf(out, "    for (const fn of this._attrUpdaters) fn();\n");
  fprintf(out, "  }\n\n");

  /* Render: build DOM */
  fprintf(out, "  _render() {\n");
  fprintf(out, "    this.innerHTML = '';\n");
  fprintf(out, "    this._exprNodes = [];\n");
  fprintf(out, "    this._attrUpdaters = [];\n\n");

  /* Inject scoped CSS */
  if (c->style_count > 0) {
    fprintf(out, "    if (!document.getElementById('forge-style-%s')) {\n",
            lname);
    fprintf(out, "      const __style = document.createElement('style');\n");
    fprintf(out, "      __style.id = 'forge-style-%s';\n", lname);
    fprintf(out, "      __style.textContent = `forge-%s {\n", tag);
    for (int i = 0; i < c->style_count; i++) {
      if (!c->style[i].is_dynamic) {
        fprintf(out, "        %s: %s;\n", c->style[i].property,
                c->style[i].value);
      }
    }
    fprintf(out, "      }`;\n");
    fprintf(out, "      document.head.appendChild(__style);\n");
    fprintf(out, "    }\n\n");
  }

  /* Emit DOM tree from template */
  _nw_id = 0;
  if (c->template_root) {
    emit_nw_html(c->template_root, "this", c, out);
  }

  fprintf(out, "  }\n\n");

  /* Lifecycle: connectedCallback */
  fprintf(out, "  connectedCallback() {\n");
  fprintf(out, "    this._syncProps();\n");
  fprintf(out, "    this._initState();\n");
  fprintf(out, "    this._render();\n");
  fprintf(out, "    this._mounted = true;\n");
  fprintf(out, "  }\n\n");

  /* Lifecycle: disconnectedCallback */
  fprintf(out, "  disconnectedCallback() {\n");
  fprintf(out, "    this._mounted = false;\n");
  fprintf(out, "  }\n\n");

  /* Lifecycle: attributeChangedCallback */
  fprintf(out, "  attributeChangedCallback(name, oldVal, newVal) {\n");
  fprintf(out, "    this._props[name] = newVal;\n");
  fprintf(out, "    if (this._mounted) { this._render(); }\n");
  fprintf(out, "  }\n\n");

  /* Sync props from attributes and JS properties */
  fprintf(out, "  _syncProps() {\n");
  for (int i = 0; i < c->prop_count; i++) {
    const char *pname = c->props[i].name;
    /* Check JS property first (set by parent component), then attribute */
    fprintf(
        out,
        "    if (this['%s'] !== undefined) this._props['%s'] = this['%s'];\n",
        pname, pname, pname);
    fprintf(out,
            "    else if (this.hasAttribute('%s')) this._props['%s'] = "
            "this.getAttribute('%s');\n",
            pname, pname, pname);
    /* Parse numeric props */
    if (c->props[i].type && (c->props[i].type->kind == TY_INT ||
                             c->props[i].type->kind == TY_FLOAT ||
                             c->props[i].type->kind == TY_DOUBLE)) {
      fprintf(out,
              "    if (this._props['%s'] !== undefined) this._props['%s'] = "
              "Number(this._props['%s']);\n",
              pname, pname, pname);
    }
  }
  fprintf(out, "  }\n\n");

  fprintf(out, "}\n\n");

  /* Register custom element */
  if (!opts || opts->web_component) {
    fprintf(out, "if (!customElements.get('forge-%s')) {\n", tag);
    fprintf(out, "  customElements.define('forge-%s', %s);\n", tag, c->name);
    fprintf(out, "}\n\n");
  }

  /* Exports */
  if (!opts || opts->es_modules) {
    fprintf(out, "export { %s };\n", c->name);
    fprintf(out, "export default %s;\n", c->name);
  }

  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 *  WASM BINDING: Original thin loader that imports .wasm module             *
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int emit_wasm_component(const ComponentNode *c,
                               const BindingOptions *opts, FILE *out) {
  char lname[256], tag[256];
  lower(lname, c->name);
  kebab(tag, c->name);

  int esm = !opts || opts->es_modules;
  int wc = !opts || opts->web_component;

  /* ── File header ── */
  fprintf(out,
          "/**\n"
          " * AUTO-GENERATED by Forge Compiler\n"
          " * Component: %s\n"
          " * DO NOT EDIT — run `forge compile %s.cx` to regenerate\n"
          " */\n\n",
          c->name, c->name);

  if (esm) {
    fprintf(out, "import { ForgeRuntime, ForgeComponent } from "
                 "'./forge-runtime.js';\n\n");
  } else {
    fprintf(out, "(function(global) {\n");
    fprintf(out,
            "  const { ForgeRuntime, ForgeComponent } = global.Forge;\n\n");
  }

  /* ── WASM loader ── */
  fprintf(out, "let __wasm_%s = null;\n\n", lname);
  fprintf(out, "async function __load_%s() {\n", lname);
  fprintf(out, "  const url = new URL('./%s.wasm', import.meta.url);\n",
          c->name);
  fprintf(out, "  const res = await fetch(url);\n");
  fprintf(out,
          "  if (!res.ok) throw new Error(`[Forge] Failed to load %s.wasm: "
          "${res.status}`);\n",
          c->name);
  fprintf(out, "  const buf  = await res.arrayBuffer();\n");
  fprintf(out, "  const env  = ForgeRuntime.wasmImports('%s');\n", lname);
  fprintf(out, "  const inst = await WebAssembly.instantiate(buf, { env });\n");
  fprintf(out, "  __wasm_%s = inst.instance.exports;\n", lname);
  fprintf(out, "  ForgeRuntime.registerExports('%s', __wasm_%s);\n", lname,
          lname);
  fprintf(out, "}\n\n");
  fprintf(out, "const __%s_ready = __load_%s();\n\n", lname, lname);

  /* ── Component class ── */
  fprintf(out, "class %s extends ForgeComponent {\n", c->name);
  fprintf(out, "  static tag      = 'forge-%s';\n", tag);
  fprintf(out, "  static wasmReady = __%s_ready;\n", lname);

  /* Prop names list */
  fprintf(out, "  static observedProps = [");
  for (int i = 0; i < c->prop_count; i++) {
    fprintf(out, "%s'%s'", i ? ", " : "", c->props[i].name);
  }
  fprintf(out, "];\n\n");

  /* mount */
  fprintf(out, "  mount(el) {\n");
  fprintf(out, "    const json = ForgeRuntime.serializeProps(this._props);\n");
  fprintf(out,
          "    __wasm_%s.forge_mount_%s(el.__forgeId, json.ptr, json.len);\n",
          lname, lname);
  fprintf(out, "  }\n\n");

  /* update */
  fprintf(out, "  update(el, newProps) {\n");
  fprintf(out, "    const json = ForgeRuntime.serializeProps(newProps);\n");
  fprintf(out,
          "    __wasm_%s.forge_update_%s(el.__forgeId, json.ptr, json.len);\n",
          lname, lname);
  fprintf(out, "  }\n\n");

  /* dispatch */
  fprintf(out, "  dispatch(el, event) {\n");
  fprintf(out, "    const ev = ForgeRuntime.serializeEvent(event);\n");
  fprintf(out, "    __wasm_%s.forge_dispatch_%s(el.__forgeId, ev.ptr);\n",
          lname, lname);
  fprintf(out, "  }\n\n");

  /* unmount */
  fprintf(out, "  unmount(el) {\n");
  fprintf(out, "    __wasm_%s.forge_unmount_%s(el.__forgeId);\n", lname, lname);
  fprintf(out, "  }\n");

  fprintf(out, "}\n\n");

  /* ── Web Component registration ── */
  if (wc) {
    fprintf(out, "if (!customElements.get('forge-%s')) {\n", tag);
    fprintf(out, "  customElements.define('forge-%s', %s);\n", tag, c->name);
    fprintf(out, "}\n\n");
  }

  if (esm) {
    fprintf(out, "export { %s };\n", c->name);
    fprintf(out, "export default %s;\n", c->name);
  } else {
    fprintf(out, "  global.Forge.components['%s'] = %s;\n", c->name, c->name);
    fprintf(out,
            "})(typeof globalThis !== 'undefined' ? globalThis : window);\n");
  }

  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 *  Public API                                                                *
 * ═══════════════════════════════════════════════════════════════════════════
 */

int binding_gen_component(const ComponentNode *c, const BindingOptions *opts,
                          FILE *out) {
  if (opts && opts->no_wasm)
    return emit_nowasm_component(c, opts, out);
  return emit_wasm_component(c, opts, out);
}

/* ─── TypeScript Declaration File ───────────────────────────────────────────
 */

int binding_gen_types(const ComponentNode *c, FILE *out) {
  char tag[256];
  kebab(tag, c->name);

  fprintf(out,
          "/**\n"
          " * AUTO-GENERATED TypeScript declarations for Forge component: %s\n"
          " */\n\n",
          c->name);

  /* Props interface */
  fprintf(out, "export interface %sProps {\n", c->name);
  for (int i = 0; i < c->prop_count; i++) {
    fprintf(out, "  %s: ", c->props[i].name);
    emit_type_ts(c->props[i].type, out);
    fprintf(out, ";\n");
  }
  fprintf(out, "}\n\n");

  /* State interface (internal, exported for testing) */
  fprintf(out, "export interface %sState {\n", c->name);
  for (int i = 0; i < c->state_count; i++) {
    fprintf(out, "  %s: ", c->state[i].name);
    emit_type_ts(c->state[i].type, out);
    fprintf(out, ";\n");
  }
  fprintf(out, "}\n\n");

  /* Component class declaration */
  fprintf(out, "export declare class %s extends HTMLElement {\n", c->name);
  for (int i = 0; i < c->prop_count; i++) {
    fprintf(out, "  %s: ", c->props[i].name);
    emit_type_ts(c->props[i].type, out);
    fprintf(out, ";\n");
  }
  fprintf(out, "  static readonly tag: 'forge-%s';\n", tag);
  fprintf(out, "  static wasmReady: Promise<void>;\n");
  fprintf(out, "}\n\n");

  /* JSX / TSX element type augmentation */
  fprintf(out,
          "declare global {\n"
          "  namespace JSX {\n"
          "    interface IntrinsicElements {\n"
          "      'forge-%s': Partial<%sProps> & { ref?: any };\n"
          "    }\n"
          "  }\n"
          "}\n",
          tag, c->name);

  return 0;
}
