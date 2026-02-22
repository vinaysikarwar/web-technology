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

static void emit_expr_js(const char *expr, FILE *out, const char *local_item) {
  const char *p = expr;
  while (*p) {
    if (local_item && strncmp(p, local_item, strlen(local_item)) == 0 &&
        !isalnum((unsigned char)p[strlen(local_item)])) {
      fprintf(out, "%s", local_item);
      p += strlen(local_item);
    } else if (strncmp(p, "state.", 6) == 0) {
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
                         const ComponentNode *comp, FILE *out,
                         const char *local_item) {
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
      fprintf(out, "      { \n");
      fprintf(out,
              "        let __tn%d = this._hydrate ? "
              "%s.querySelector('[data-fexpr=\"%d\"]') : null;\n",
              id, parent_var, id);
      fprintf(out,
              "        if (this._hydrate && !__tn%d) console.warn(`Forge: "
              "Hydration target fexpr-%d not found in`, %s);\n",
              id, id, parent_var);
      fprintf(out,
              "        if (!__tn%d) __tn%d = document.createTextNode('');\n",
              id, id);
      /* Build a reactive updater */
      fprintf(out, "        __tn%d.__forgeUpdate = () => {\n", id);
      fprintf(out, "          const __val = String(");
      emit_expr_js(n->text, out, local_item);
      fprintf(out, ");\n");
      fprintf(out,
              "          if (__tn%d.textContent !== __val) __tn%d.textContent "
              "= __val;\n",
              id, id);
      fprintf(out, "        };\n");
      fprintf(out, "        __tn%d.__forgeUpdate();\n", id);
      fprintf(out, "        this._exprNodes.push(__tn%d);\n", id);
      fprintf(out, "        if (!this._hydrate) {\n");
      fprintf(out,
              "          if (__tn%d.setAttribute) "
              "__tn%d.setAttribute('data-fexpr', '%d');\n",
              id, id, id);
      fprintf(out, "          %s.appendChild(__tn%d);\n", parent_var, id);
      fprintf(out, "        }\n");
      fprintf(out, "      }\n");
    }
    break;

  case HTML_COMPONENT: {
    int id = _nw_id++;
    char ctag[256];
    kebab(ctag, n->tag ? n->tag : "div");
    fprintf(out, "      { \n");
    /* Try to reuse existing element during hydration; fall back to createElement
     * if querySelector returns null (e.g. SSR content has no data-fid markers) */
    fprintf(out,
            "        let __cc%d = this._hydrate ? "
            "%s.querySelector(':scope > forge-%s[data-fid=\"%d\"]') : null;\n",
            id, parent_var, ctag, id);
    fprintf(out, "        const __cc_new%d = !__cc%d;\n", id, id);
    fprintf(out, "        if (!__cc%d) {\n", id);
    fprintf(out, "          __cc%d = document.createElement('forge-%s');\n", id, ctag);
    fprintf(out, "          __cc%d.setAttribute('data-fid', '%d');\n", id, id);
    fprintf(out, "        }\n");
    /* Set props BEFORE appendChild so connectedCallback sees them */
    for (int i = 0; i < n->attr_count; i++) {
      const char *aname = n->attrs[i].name;
      const char *aval = n->attrs[i].value ? n->attrs[i].value : "";
      if (n->attrs[i].is_expr) {
        fprintf(out, "        __cc%d['%s'] = ", id, aname);
        emit_expr_js(aval, out, local_item);
        fprintf(out, ";\n");
      } else {
        fprintf(out, "        __cc%d.setAttribute('%s', ", id, aname);
        emit_js_str(aval, out);
        fprintf(out, ");\n");
      }
    }
    /* Append AFTER props are set so connectedCallback has full prop data */
    fprintf(out, "        if (__cc_new%d) %s.appendChild(__cc%d);\n",
            id, parent_var, id);

    /* For singleton components (not inside a <for> loop), register a prop
     * updater so app._refresh() keeps props in sync with parent state.
     * Inside <for> loops (local_item != NULL) the loop's own attrUpdater
     * recreates child elements with fresh props on each refresh. */
    if (!local_item) {
      int has_expr = 0;
      for (int i = 0; i < n->attr_count; i++)
        if (n->attrs[i].is_expr) { has_expr = 1; break; }
      if (has_expr) {
        fprintf(out, "      ((ref) => {\n");
        fprintf(out, "        const __ae%d_p = () => {\n", id);
        for (int i = 0; i < n->attr_count; i++) {
          const char *aname = n->attrs[i].name;
          const char *aval  = n->attrs[i].value ? n->attrs[i].value : "";
          if (n->attrs[i].is_expr) {
            fprintf(out, "          ref['%s'] = ", aname);
            emit_expr_js(aval, out, NULL);
            fprintf(out, ";\n");
          }
        }
        fprintf(out, "        };\n");
        fprintf(out, "        this._attrUpdaters.push(__ae%d_p);\n", id);
        fprintf(out, "      })(__cc%d);\n", id);
      }
    }

    fprintf(out, "      }\n");
    break;
  }

  case HTML_ELEMENT: {
    int id = _nw_id++;
    fprintf(out,
            "      const __e%d = this._hydrate ? "
            "(%s.querySelector('[data-fid=\"%d\"]') || "
            "document.createElement('%s')) : document.createElement('%s');\n",
            id, parent_var, id, n->tag ? n->tag : "div",
            n->tag ? n->tag : "div");

    /* Attributes */
    for (int i = 0; i < n->attr_count; i++) {
      const char *aname = n->attrs[i].name;
      const char *aval = n->attrs[i].value ? n->attrs[i].value : "";

      /* Event binding: onclick={...} */
      if (strncmp(aname, "on", 2) == 0 && islower((unsigned char)aname[2])) {
        fprintf(out, "      __e%d.addEventListener('%s', (e) => {\n", id,
                aname + 2);
        fprintf(
            out,
            "        const state = this._state; const props = this._props;\n");
        const char *h = aval;
        if (*h == '@') {
          fprintf(out, "        this._handle_%s(e);\n", h + 1);
        } else {
          /* Handle inline code: replace @name with this._handle_name(e) */
          const char *p = h;
          while (*p) {
            if (*p == '@') {
              p++;
              fprintf(out, "this._handle_");
              while (isalnum((unsigned char)*p) || *p == '_') {
                fputc(*p, out);
                p++;
              }
              fprintf(out, "(e)");
            } else {
              fputc(*p, out);
              p++;
            }
          }
          fprintf(out, ";\n");
        }
        fprintf(out, "        this._refresh();\n");
        fprintf(out, "      });\n");
      } else if (n->attrs[i].is_expr) {
        int aid = _nw_id++;
        fprintf(out, "      { const __ae%d = () => {\n", aid);
        fprintf(out, "          __e%d.setAttribute('%s', String(", id, aname);
        emit_expr_js(aval, out, local_item);
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

    if (id == 0) {
      /* Root element of component template */
    }

    /* Recurse children */
    char child_var[64];
    snprintf(child_var, sizeof(child_var), "__e%d", id);
    for (int i = 0; i < n->child_count; i++) {
      /* Merge consecutive HTML_TEXT children into one text node */
      if (n->children[i].kind == HTML_TEXT && n->children[i].text) {
        int j = i;
        while (j < n->child_count && n->children[j].kind == HTML_TEXT &&
               n->children[j].text)
          j++;
        if (j > i) {
          fprintf(out,
                  "      if (!this._hydrate) "
                  "%s.appendChild(document.createTextNode(",
                  child_var);
          int first = 1;
          for (int k = i; k < j; k++) {
            const char *t = n->children[k].text;
            int all_ws = 1;
            for (const char *c = t; *c; c++) {
              if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') {
                all_ws = 0;
                break;
              }
            }
            if (all_ws)
              continue;
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
            fprintf(out, "''");
          fprintf(out, "));\n");
          i = j - 1;
          continue;
        }
      }
      emit_nw_html(&n->children[i], child_var, comp, out, local_item);
    }

    fprintf(out, "      if (!this._hydrate) %s.appendChild(__e%d);\n",
            parent_var, id);
    fprintf(out,
            "      if (!this._hydrate) __e%d.setAttribute('data-fid', '%d');\n",
            id, id);
    break;
  }

  case HTML_IF: {
    int id = _nw_id++;
    char *condition = "true";
    for (int i = 0; i < n->attr_count; i++) {
      if (strcmp(n->attrs[i].name, "condition") == 0) {
        condition = n->attrs[i].value;
        break;
      }
    }
    fprintf(out, "      { \n");
    fprintf(out,
            "        const __e%d = this._hydrate ? (%s.querySelector(':scope > "
            "[data-fif=\"%d\"]') || document.createElement('div')) : "
            "document.createElement('div');\n",
            id, parent_var, id);
    fprintf(out, "        __e%d.style.display = 'contents';\n", id);
    fprintf(out, "        if (!this._hydrate) {\n");
    fprintf(out, "          __e%d.setAttribute('data-fif', '%d');\n", id, id);
    fprintf(out, "          %s.appendChild(__e%d);\n", parent_var, id);
    fprintf(out, "        }\n");
    /* Reactive toggle */
    fprintf(out, "        const __ae%d = () => {\n", id);
    fprintf(out, "          __e%d.style.display = (", id);
    emit_expr_js(condition, out, local_item);
    fprintf(out, ") ? 'contents' : 'none';\n");
    fprintf(out, "        };\n");
    fprintf(out, "        __ae%d();\n", id);
    fprintf(out, "        this._attrUpdaters.push(__ae%d);\n", id);

    /* Children */
    char child_var[32];
    sprintf(child_var, "__e%d", id);
    for (int i = 0; i < n->child_count; i++) {
      emit_nw_html(&n->children[i], child_var, comp, out, local_item);
    }
    fprintf(out, "      }\n");
    break;
  }

  case HTML_FOR: {
    int id = _nw_id++;
    char *each = "[]";
    char *as = "item";
    for (int i = 0; i < n->attr_count; i++) {
      if (strcmp(n->attrs[i].name, "each") == 0)
        each = n->attrs[i].value;
      if (strcmp(n->attrs[i].name, "as") == 0)
        as = n->attrs[i].value;
    }
    fprintf(out, "      const __e%d = document.createElement('div');\n", id);
    fprintf(out, "      __e%d.style.display = 'contents';\n", id);
    fprintf(out, "      %s.appendChild(__e%d);\n", parent_var, id);
    fprintf(out, "      { \n");
    fprintf(out, "        const __ae%d = () => {\n", id);
    fprintf(out, "          __e%d.innerHTML = '';\n", id);
    fprintf(out, "          const __list = ");
    emit_expr_js(each, out, local_item);
    fprintf(out, ";\n");
    fprintf(out, "          if (Array.isArray(__list)) {\n");
    fprintf(out, "            __list.forEach((%s) => {\n", as);
    char _for_var[64];
    snprintf(_for_var, sizeof(_for_var), "__e%d", id);
    for (int i = 0; i < n->child_count; i++) {
      emit_nw_html(&n->children[i], _for_var, comp, out, as);
    }
    fprintf(out, "            });\n");
    fprintf(out, "          }\n");
    fprintf(out, "        };\n");
    fprintf(out, "        __ae%d();\n", id);
    fprintf(out, "        this._attrUpdaters.push(__ae%d);\n", id);
    fprintf(out, "      }\n");
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

  /* Propertie Accessors */
  for (int i = 0; i < c->prop_count; i++) {
    const char *pname = c->props[i].name;
    fprintf(out, "  get %s() { return this._props['%s']; }\n", pname, pname);
    fprintf(out, "  set %s(val) {\n", pname);
    fprintf(out, "    this._props['%s'] = val;\n", pname);
    /* Cast if numeric */
    if (c->props[i].type && (c->props[i].type->kind == TY_INT ||
                             c->props[i].type->kind == TY_FLOAT ||
                             c->props[i].type->kind == TY_DOUBLE)) {
      fprintf(out,
              "    if (val !== undefined) this._props['%s'] = Number(val);\n",
              pname);
    }
    fprintf(out, "    if (this._mounted) this._refresh();\n");
    fprintf(out, "  }\n\n");
  }

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
            /* skip comma + whitespace before first argument */
            if (*p == ',') p++;
            while (*p == ' ') p++;

            /* Parse each argument individually (split by ',' at depth==1).
             * forge_sprintf("fmt", a, b, c) → args[0]="a", args[1]="b" ... */
            char args[8][512];
            int  nargs = 0;
            int  depth = 1; /* already inside forge_sprintf( */
            while (*p && depth > 0 && nargs < 8) {
              int ai = 0;
              while (*p && ai < 510) {
                if (*p == '(') {
                  depth++;
                  args[nargs][ai++] = *p++;
                } else if (*p == ')') {
                  depth--;
                  if (depth == 0) break; /* end of forge_sprintf() */
                  args[nargs][ai++] = *p++;
                } else if (*p == ',' && depth == 1) {
                  p++; /* skip comma */
                  while (*p == ' ') p++;
                  break; /* end of this arg */
                } else {
                  args[nargs][ai++] = *p++;
                }
              }
              /* trim trailing whitespace */
              while (ai > 0 && (args[nargs][ai-1] == ' ' ||
                                args[nargs][ai-1] == '\t'))
                args[nargs][--ai] = '\0';
              args[nargs][ai] = '\0';
              if (ai > 0) nargs++;
              if (depth == 0) break;
            }

            /* Build JS: (() => { const __v0 = arg0, __v1 = arg1; return `...`; })()
             * Each %specifier consumes the next __vN in order. */
            fprintf(out, "(() => { ");
            if (nargs > 0) {
              fprintf(out, "const ");
              for (int i = 0; i < nargs; i++) {
                if (i > 0) fprintf(out, ", ");
                fprintf(out, "__v%d = ", i);
                emit_expr_js(args[i], out, NULL);
              }
              fprintf(out, "; ");
            }
            fprintf(out, "return `");
            int vi = 0;
            for (const char *f = fmt; *f; f++) {
              if (*f == '%') {
                f++; /* skip '%' */
                /* skip flags: -, +, space, 0, # */
                while (*f == '-' || *f == '+' || *f == ' ' ||
                       *f == '0'  || *f == '#') f++;
                /* skip width digits */
                while (*f >= '0' && *f <= '9') f++;
                /* optional precision: .N */
                int prec = -1;
                if (*f == '.') {
                  f++;
                  prec = 0;
                  while (*f >= '0' && *f <= '9') {
                    prec = prec * 10 + (*f - '0');
                    f++;
                  }
                }
                /* f now at conversion char */
                if (*f == 'f' || *f == 'e' || *f == 'g') {
                  /* Coerce with (+x||0) so string prices or undefined don't crash */
                  if (prec >= 0) fprintf(out, "${(+__v%d||0).toFixed(%d)}", vi, prec);
                  else           fprintf(out, "${(+__v%d||0).toFixed(2)}", vi);
                } else if (*f == 'd' || *f == 'i' || *f == 'u') {
                  fprintf(out, "${Math.floor(+__v%d||0)}", vi);
                } else if (*f == 's') {
                  fprintf(out, "${__v%d}", vi);
                } else if (*f) {
                  /* unknown specifier — emit literally */
                  fprintf(out, "%%");
                  fputc(*f, out);
                }
                vi++;
              } else if (*f == '`') {
                fprintf(out, "\\`");
              } else {
                fputc(*f, out);
              }
            }
            fprintf(out, "`; })()");
          } else {
            /* Fallback: just emit as-is */
            emit_expr_js(expr, out, NULL);
          }
          /* ↑ forge_sprintf branch ends here — do NOT emit expr again */
        } else {
          /* Non-forge_sprintf: translate C expression directly to JS */
          emit_expr_js(expr, out, NULL);
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

  /* Render: build or hydrate DOM */
  fprintf(out, "  _render() {\n");
  fprintf(out, "    this._hydrate = this.innerHTML.trim() !== '';\n");
  fprintf(out, "    this._exprNodes = [];\n");
  fprintf(out, "    this._attrUpdaters = [];\n\n");
  fprintf(
      out,
      "    if (this._hydrate) { console.log(`Forge: Hydrating "
      "${this.localName} with ${this.attributes.length} attributes`); }\n\n");

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
    emit_nw_html(c->template_root, "this", c, out, NULL);
  }

  fprintf(out, "    this._refresh();\n");
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
  fprintf(out, "    /* Priority: 1. Existing JS property, 2. Attribute */\n");
  fprintf(out, "    for (const p of %s.observedAttributes) {\n", c->name);
  fprintf(out, "      if (this[p] !== undefined) {\n");
  fprintf(out, "        this._props[p] = this[p];\n");
  fprintf(out, "      } else if (this.hasAttribute(p)) {\n");
  fprintf(out, "        this._props[p] = this.getAttribute(p);\n");
  fprintf(out, "      }\n");
  fprintf(out, "    }\n");
  /* Parsing numeric props */
  for (int i = 0; i < c->prop_count; i++) {
    if (c->props[i].type && (c->props[i].type->kind == TY_INT ||
                             c->props[i].type->kind == TY_FLOAT ||
                             c->props[i].type->kind == TY_DOUBLE)) {
      fprintf(out,
              "    if (this._props['%s'] !== undefined) this._props['%s'] = "
              "Number(this._props['%s']);\n",
              c->props[i].name, c->props[i].name, c->props[i].name);
    }
  }
  fprintf(out, "  }\n\n"
               "  /* State Getters/Setters */\n");
  for (int i = 0; i < c->state_count; i++) {
    fprintf(out, "  get %s() { return this._state.%s; }\n", c->state[i].name,
            c->state[i].name);
    fprintf(out, "  set %s(val) {\n", c->state[i].name);
    fprintf(out, "    this._state.%s = val;\n", c->state[i].name);
    fprintf(out, "    this._refresh();\n");
    fprintf(out, "  }\n\n");
  }

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

/* ═══════════════════════════════════════════════════════════════════════════
 * * WASM BINDING: Original thin loader that imports .wasm module *
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

/* ═══════════════════════════════════════════════════════════════════════════
 * * Public API *
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
/* ─── Pre-rendering (SSG) ───────────────────────────────────────────────────
 */

static int _prerender_id = 0;

static void emit_prerender_html_recursive(const HtmlNode *n,
                                          const ComponentNode **registry,
                                          int registry_count, FILE *out) {
  if (!n)
    return;
  int id = _prerender_id++;

  switch (n->kind) {
  case HTML_TEXT:
    if (n->text)
      fprintf(out, "%s", n->text);
    break;

  case HTML_EXPR:
    /* SSG output for expression: wrap in a marker so hydration can find it */
    fprintf(out, "<span data-fexpr=\"%d\"></span>", id);
    break;

  case HTML_COMPONENT: {
    char tag[256];
    kebab(tag, n->tag);
    fprintf(out, "<forge-%s data-fid=\"%d\"", tag, id);
    for (int i = 0; i < n->attr_count; i++) {
      if (!n->attrs[i].is_expr) {
        fprintf(out, " %s=\"%s\"", n->attrs[i].name,
                n->attrs[i].value ? n->attrs[i].value : "");
      }
    }
    fprintf(out, ">");
    /* Inline child component template if found in registry */
    const ComponentNode *target = NULL;
    for (int i = 0; i < registry_count; i++) {
      if (registry[i] && strcmp(registry[i]->name, n->tag) == 0) {
        target = registry[i];
        break;
      }
    }
    if (target && target->template_root) {
      int prev_id = _prerender_id;
      _prerender_id = 0;
      emit_prerender_html_recursive(target->template_root, registry,
                                    registry_count, out);
      _prerender_id = prev_id;
    }
    fprintf(out, "</forge-%s>", tag);
    break;
  }

  case HTML_ELEMENT: {
    fprintf(out, "<%s data-fid=\"%d\"", n->tag ? n->tag : "div", id);
    for (int i = 0; i < n->attr_count; i++) {
      if (!n->attrs[i].is_expr) {
        fprintf(out, " %s=\"%s\"", n->attrs[i].name,
                n->attrs[i].value ? n->attrs[i].value : "");
      }
    }
    fprintf(out, ">");
    for (int i = 0; i < n->child_count; i++) {
      emit_prerender_html_recursive(&n->children[i], registry, registry_count,
                                    out);
    }
    fprintf(out, "</%s>", n->tag ? n->tag : "div");
    return;
  }

  case HTML_IF: {
    fprintf(out, "<div data-fif=\"%d\" style=\"display:contents\">", id);
    for (int i = 0; i < n->child_count; i++) {
      emit_prerender_html_recursive(&n->children[i], registry, registry_count,
                                    out);
    }
    fprintf(out, "</div>");
    break;
  }

  case HTML_FOR:
    break;
  }
}

int binding_gen_prerender(const ComponentNode *c,
                          const ComponentNode **registry, int registry_count,
                          FILE *out) {
  if (!c || !c->template_root)
    return 0;
  _prerender_id = 0;
  emit_prerender_html_recursive(c->template_root, registry, registry_count,
                                out);
  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SSR RENDERER GENERATOR
 * Generates ComponentName.forge.ssr.js — a Node.js module that exports
 *   render(state, props) => HTML string
 * No browser APIs (document / window / customElements) used.
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Emit a Forge expression for SSR context.
 * state.X, props.X, computed.X are kept as-is (function parameters match). */
static void emit_ssr_expr(const char *expr, FILE *out) {
  if (!expr) { fprintf(out, "''"); return; }
  /* computed.X has no meaning in SSR — emit empty string */
  if (strncmp(expr, "computed.", 9) == 0) { fprintf(out, "''"); return; }
  fprintf(out, "%s", expr);
}

/* Forward-declare so emit_ssr_children and emit_ssr_node can call each other */
static void emit_ssr_node(const HtmlNode *n, const ComponentNode **registry,
                          int rc, int depth, FILE *out);

static void emit_ssr_children(const HtmlNode *parent,
                               const ComponentNode **registry, int rc,
                               int depth, FILE *out) {
  for (int i = 0; i < parent->child_count; i++)
    emit_ssr_node(&parent->children[i], registry, rc, depth, out);
}

/* Indentation helper (max 8 levels) */
static const char *_ssrind(int d) {
  static const char *T[] = {
    "", "  ", "    ", "      ", "        ",
    "          ", "            ", "              "
  };
  return T[d < 0 ? 0 : d > 7 ? 7 : d];
}

static void emit_ssr_node(const HtmlNode *n, const ComponentNode **registry,
                          int rc, int depth, FILE *out) {
  if (!n) return;
  const char *ind = _ssrind(depth);

  switch (n->kind) {

  case HTML_TEXT:
    if (n->text && n->text[0]) {
      fprintf(out, "%s__h += ", ind);
      emit_js_str(n->text, out);
      fprintf(out, ";\n");
    }
    break;

  case HTML_EXPR:
    if (n->text) {
      fprintf(out, "%s__h += _e(", ind);
      emit_ssr_expr(n->text, out);
      fprintf(out, ");\n");
    }
    break;

  case HTML_ELEMENT: {
    /* Tag open */
    fprintf(out, "%s__h += '<%s';\n", ind, n->tag ? n->tag : "div");
    /* Attributes */
    for (int i = 0; i < n->attr_count; i++) {
      const Attribute *a = &n->attrs[i];
      if (strncmp(a->name, "on", 2) == 0) continue; /* skip event handlers */
      if (!a->is_expr) {
        /* Static attribute — emit as JS string literal */
        fprintf(out, "%s__h += ' %s=\"", ind, a->name);
        if (a->value) {
          for (const char *p = a->value; *p; p++) {
            if (*p == '"')  fputs("\\\"", out);
            else if (*p == '\\') fputs("\\\\", out);
            else if (*p == '\'') fputs("\\'",  out);
            else fputc(*p, out);
          }
        }
        fprintf(out, "\"';\n");
      } else {
        /* Dynamic attribute — evaluate and escape */
        fprintf(out, "%s__h += ' %s=\"' + _e(", ind, a->name);
        emit_ssr_expr(a->value ? a->value : "", out);
        fprintf(out, ") + '\"';\n");
      }
    }
    fprintf(out, "%s__h += '>';\n", ind);
    emit_ssr_children(n, registry, rc, depth, out);
    if (!n->self_closing)
      fprintf(out, "%s__h += '</%s>';\n", ind, n->tag ? n->tag : "div");
    break;
  }

  case HTML_COMPONENT: {
    /* Look up child component in registry */
    int found = 0;
    for (int i = 0; i < rc; i++) {
      if (registry[i] && n->tag && strcmp(registry[i]->name, n->tag) == 0) {
        found = 1; break;
      }
    }
    if (found) {
      fprintf(out, "%s__h += _render%s({", ind, n->tag);
      for (int i = 0; i < n->attr_count; i++) {
        const Attribute *a = &n->attrs[i];
        fprintf(out, "'%s': (", a->name);
        if (a->is_expr) emit_ssr_expr(a->value ? a->value : "''", out);
        else            emit_js_str(a->value ? a->value : "", out);
        fprintf(out, "), ");
      }
      fprintf(out, "});\n");
    }
    break;
  }

  case HTML_IF: {
    const char *cond = NULL;
    for (int i = 0; i < n->attr_count; i++)
      if (strcmp(n->attrs[i].name, "condition") == 0) { cond = n->attrs[i].value; break; }
    if (cond) { fprintf(out, "%sif (", ind); emit_ssr_expr(cond, out); fprintf(out, ") {\n"); }
    else        fprintf(out, "%s{\n", ind);
    emit_ssr_children(n, registry, rc, depth + 1, out);
    fprintf(out, "%s}\n", ind);
    break;
  }

  case HTML_FOR: {
    const char *each = NULL, *as_var = NULL;
    for (int i = 0; i < n->attr_count; i++) {
      if (strcmp(n->attrs[i].name, "each") == 0) each   = n->attrs[i].value;
      if (strcmp(n->attrs[i].name, "as")   == 0) as_var = n->attrs[i].value;
    }
    if (each && as_var) {
      fprintf(out, "%sfor (const %s of (", ind, as_var);
      emit_ssr_expr(each, out);
      fprintf(out, " || [])) {\n");
    } else {
      fprintf(out, "%s{\n", ind);
    }
    emit_ssr_children(n, registry, rc, depth + 1, out);
    fprintf(out, "%s}\n", ind);
    break;
  }
  }
}

int binding_gen_ssr_js(const ComponentNode *c,
                        const ComponentNode **registry, int registry_count,
                        FILE *out) {
  if (!c) return 1;

  fprintf(out,
    "/**\n"
    " * AUTO-GENERATED by Forge Compiler — SSR Renderer\n"
    " * Component: %s  (Node.js, no browser APIs)\n"
    " * Usage: const { render } = require('./%s.forge.ssr.js');\n"
    " *        const html = render(state, props);\n"
    " */\n"
    "'use strict';\n\n",
    c->name, c->name);

  /* HTML-escape helper */
  fprintf(out,
    "const _e = v => v == null ? '' : String(v)\n"
    "  .replace(/&/g, '&amp;').replace(/</g, '&lt;')\n"
    "  .replace(/>/g, '&gt;').replace(/\"/g, '&quot;');\n\n");

  /* Emit helper renderers for every child component in the registry */
  for (int i = 0; i < registry_count; i++) {
    const ComponentNode *ch = registry[i];
    if (!ch || strcmp(ch->name, c->name) == 0) continue;

    fprintf(out, "function _render%s(props) {\n", ch->name);
    fprintf(out, "  if (!props) props = {};\n");
    fprintf(out, "  const state    = props; /* child: props === state in SSR */\n");
    fprintf(out, "  const computed = {}; /* computed fields are no-ops in SSR */\n");
    fprintf(out, "  let __h = '';\n");
    if (ch->template_root)
      emit_ssr_node(ch->template_root, registry, registry_count, 1, out);
    fprintf(out, "  return __h;\n}\n\n");
  }

  /* Main render(state, props) function */
  fprintf(out, "function render(state, props) {\n");
  fprintf(out, "  if (!state) state = {};\n");
  fprintf(out, "  if (!props) props = {};\n");
  fprintf(out, "  const computed = {}; /* computed fields are no-ops in SSR */\n");
  fprintf(out, "  let __h = '';\n");
  if (c->template_root)
    emit_ssr_node(c->template_root, registry, registry_count, 1, out);
  fprintf(out, "  return __h;\n}\n\n");

  fprintf(out, "module.exports = { render };\n");
  return 0;
}

/* ─── SSR HTTP Server Generator ─────────────────────────────────────────────
 * Emits forge-ssr-server.js — a ready-to-run Node.js SSR server.
 * Users only need to fill in resolveState() with their API calls.
 * Everything else (HTTP, static files, proxy, injection) is pre-generated.
 */
int binding_gen_ssr_server(const ComponentNode *c,
                            const ComponentNode **registry, int registry_count,
                            FILE *out) {
  if (!c) return 1;
  (void)registry; (void)registry_count; /* reserved for future cross-component SSR */

  char tag[256];
  kebab(tag, c->name);

  /* ── File header ── */
  fprintf(out,
    "'use strict';\n"
    "/**\n"
    " * AUTO-GENERATED by Forge Compiler (--ssr)\n"
    " * Forge SSR Server — forge-ssr-server.js\n"
    " * Component : %s  (<forge-%s>)\n"
    " *\n"
    " * USAGE:\n"
    " *   node dist/forge-ssr-server.js\n"
    " *\n"
    " * ENV VARS:\n"
    " *   PORT=3000  API_BASE=http://localhost:8000  API_TOKEN=<jwt>\n"
    " *\n"
    " * EDIT resolveState() below to fetch your API data per route.\n"
    " * Everything else is auto-generated — do not edit other sections.\n"
    " */\n\n",
    c->name, tag);

  /* ── Node.js requires ── */
  fprintf(out,
    "const http   = require('http');\n"
    "const https  = require('https');\n"
    "const fs     = require('fs');\n"
    "const path   = require('path');\n"
    "const urlMod = require('url');\n\n");

  /* ── Configuration ── */
  fprintf(out,
    "/* ── Configuration (override via env vars) ─────────────────────────── */\n"
    "const PORT      = parseInt(process.env.PORT      || '3000', 10);\n"
    "const API_BASE  = process.env.API_BASE  || 'http://localhost:8000';\n"
    "const API_TOKEN = process.env.API_TOKEN || '';\n"
    "const DIST_DIR  = __dirname;\n"
    "const ROOT_DIR  = path.resolve(DIST_DIR, '..');\n\n");

  /* ── Component renderer reference ── */
  fprintf(out,
    "/* ── Component renderer (auto-generated, do not edit) ──────────────── */\n"
    "const { render } = require('./%s.forge.ssr.js');\n\n",
    c->name);

  /* ── MIME table ── */
  fprintf(out,
    "/* ── MIME types ────────────────────────────────────────────────────── */\n"
    "const MIME = {\n"
    "  '.js':'application/javascript; charset=utf-8',\n"
    "  '.mjs':'application/javascript; charset=utf-8',\n"
    "  '.json':'application/json',\n"
    "  '.css':'text/css; charset=utf-8',\n"
    "  '.html':'text/html; charset=utf-8',\n"
    "  '.png':'image/png', '.jpg':'image/jpeg', '.jpeg':'image/jpeg',\n"
    "  '.svg':'image/svg+xml', '.ico':'image/x-icon', '.woff2':'font/woff2',\n"
    "};\n\n");

  /* ── Internal apiFetch helper ── */
  fprintf(out,
    "/* ── Internal API fetch (Node → your backend) ──────────────────────── */\n"
    "function apiFetch(endpoint) {\n"
    "  return new Promise((resolve, reject) => {\n"
    "    const fullUrl = API_BASE + endpoint;\n"
    "    const parsed  = new urlMod.URL(fullUrl);\n"
    "    const isHttps = parsed.protocol === 'https:';\n"
    "    const mod = isHttps ? https : http;\n"
    "    const req = mod.request({\n"
    "      hostname: parsed.hostname,\n"
    "      port:     parsed.port || (isHttps ? 443 : 80),\n"
    "      path:     parsed.pathname + parsed.search,\n"
    "      method:   'GET',\n"
    "      headers:  { 'Accept': 'application/json',\n"
    "                  ...(API_TOKEN ? { Authorization: 'Bearer ' + API_TOKEN } : {}) },\n"
    "    }, res => {\n"
    "      let body = '';\n"
    "      res.setEncoding('utf8');\n"
    "      res.on('data', d => body += d);\n"
    "      res.on('end', () => {\n"
    "        try { resolve({ status: res.statusCode, data: JSON.parse(body) }); }\n"
    "        catch (e) { reject(new Error('JSON parse failed: ' + endpoint)); }\n"
    "      });\n"
    "    });\n"
    "    req.on('error', reject);\n"
    "    req.setTimeout(8000, () => req.destroy(new Error('Timeout: ' + endpoint)));\n"
    "    req.end();\n"
    "  });\n"
    "}\n\n");

  /* ── resolveState() — user editable section ── */
  fprintf(out,
    "/* ═══════════════════════════════════════════════════════════════════\n"
    " *  RESOLVE STATE  ←  EDIT THIS FUNCTION\n"
    " *\n"
    " *  Called on every page request. Fetch your API data here.\n"
    " *\n"
    " *  `route`  — URL pathname, e.g. '/', '/products', '/item/my-slug'\n"
    " *\n"
    " *  Return:\n"
    " *    state — object matching @state fields of %s:\n",
    c->name);

  /* Emit state field hints as comments */
  for (int i = 0; i < c->state_count; i++) {
    const Field *f = &c->state[i];
    const char *ts = "any";
    if (f->type) {
      switch (f->type->kind) {
        case TY_INT: case TY_LONG: case TY_SHORT: case TY_UNSIGNED: ts = "number"; break;
        case TY_FLOAT: case TY_DOUBLE: ts = "number"; break;
        case TY_CHAR:  ts = "string";  break;
        case TY_BOOL:  ts = "boolean"; break;
        default: ts = "any"; break;
      }
      if (f->type->kind == TY_PTR && f->type->inner &&
          f->type->inner->kind == TY_CHAR) ts = "string";
    }
    fprintf(out, " *      %-24s (%s)\n", f->name, ts);
  }

  fprintf(out,
    " *    meta  — { title, desc, ogType }   updates <head> tags\n"
    " *    data  — JSON seeded into window.__SSR_DATA__  (skips client re-fetch)\n"
    " * ═══════════════════════════════════════════════════════════════════ */\n"
    "async function resolveState(route) {\n"
    "  // Example — uncomment and adapt:\n"
    "  //\n"
    "  // if (route === '/') {\n"
    "  //   const { data: items } = await apiFetch('/api/items/');\n"
    "  //   return {\n"
    "  //     state: { page: 0, products: items.results },\n"
    "  //     meta:  { title: 'Home — My App', desc: 'Shop online' },\n"
    "  //     data:  { items: items.results, total: items.count },\n"
    "  //   };\n"
    "  // }\n"
    "  //\n"
    "  // const m = route.match(/^\\/item\\/(.+)$/);\n"
    "  // if (m) {\n"
    "  //   const { data: item } = await apiFetch('/api/items/?slug=' + m[1]);\n"
    "  //   return {\n"
    "  //     state: { page: 2, det_name: item.name, det_price: item.price },\n"
    "  //     meta:  { title: item.name + ' — My App', desc: item.description },\n"
    "  //     data:  { item },\n"
    "  //   };\n"
    "  // }\n"
    "\n"
    "  return {\n"
    "    state: { page: 0 },\n"
    "    meta:  { title: 'Forge App', desc: '' },\n"
    "    data:  {},\n"
    "  };\n"
    "}\n\n");

  /* ── HTML helpers ── */
  fprintf(out,
    "/* ── HTML helpers (auto-generated) ─────────────────────────────────── */\n"
    "function _esc(s) {\n"
    "  return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;')\n"
    "                      .replace(/>/g,'&gt;').replace(/\"/g,'&quot;');\n"
    "}\n\n"
    "function _buildPage(template, html, meta, data) {\n"
    "  const title = meta.title || 'Forge App';\n"
    "  const desc  = meta.desc  || '';\n"
    "  let out = template;\n"
    "  out = out.replace(/<title>[^<]*<\\/title>/, `<title>${_esc(title)}</title>`);\n"
    "  out = out.replace(/(<meta\\s+name=\"description\"\\s+content=\")[^\"]*(\")/, `$1${_esc(desc)}$2`);\n"
    "  out = out.replace(/(<meta\\s+property=\"og:title\"\\s+content=\")[^\"]*(\")/, `$1${_esc(title)}$2`);\n"
    "  out = out.replace(/(<meta\\s+property=\"og:description\"\\s+content=\")[^\"]*(\")/, `$1${_esc(desc)}$2`);\n"
    "  out = out.replace(/(<meta\\s+name=\"twitter:title\"\\s+content=\")[^\"]*(\")/, `$1${_esc(title)}$2`);\n"
    "  if (meta.ogType)\n"
    "    out = out.replace(/(<meta\\s+property=\"og:type\"\\s+content=\")[^\"]*(\")/, `$1${_esc(meta.ogType)}$2`);\n"
    "\n"
    "  /* Inject SSR data + customElements clear-patch */\n"
    "  const ssrJson = JSON.stringify(data||{}).replace(/<\\/script/gi,'<\\\\/script');\n"
    "  const patch = `<script>window.__SSR_DATA__=${ssrJson};`\n"
    "    + `(function(){var o=customElements.define.bind(customElements);`\n"
    "    + `customElements.define=function(n,c,x){`\n"
    "    + `if(n==='forge-%s'){var e=document.getElementById('app');if(e)e.innerHTML='';}return o(n,c,x);};})();`\n"
    "    + `</script>`;\n"
    "  out = out.replace('</head>', patch + '\\n</head>');\n"
    "\n"
    "  /* Inject SSR HTML into <forge-app> */\n"
    "  if (html) {\n"
    "    out = out.replace(\n"
    "      /<forge-%s(\\s+[^>]*)?>\\s*<\\/forge-%s>/,\n"
    "      `<forge-%s id=\"app\">\\n<!-- SSR: pre-rendered -->${html}\\n</forge-%s>`\n"
    "    );\n"
    "  }\n"
    "  return out;\n"
    "}\n\n",
    tag, tag, tag, tag, tag);

  /* ── API proxy ── */
  fprintf(out,
    "/* ── API proxy ─────────────────────────────────────────────────────── */\n"
    "function _proxyApi(req, res) {\n"
    "  const target = new urlMod.URL(API_BASE + req.url);\n"
    "  const isHttps = target.protocol === 'https:';\n"
    "  const mod = isHttps ? https : http;\n"
    "  let body = [];\n"
    "  req.on('data', c => body.push(c));\n"
    "  req.on('end', () => {\n"
    "    const buf = Buffer.concat(body);\n"
    "    const hdrs = { Accept: 'application/json', Host: target.host,\n"
    "                   ...(API_TOKEN ? { Authorization: 'Bearer ' + API_TOKEN } : {}),\n"
    "                   ...(req.headers['content-type'] ? { 'Content-Type': req.headers['content-type'] } : {}) };\n"
    "    if (buf.length) hdrs['Content-Length'] = buf.length;\n"
    "    const pr = mod.request({ hostname: target.hostname,\n"
    "      port: target.port || (isHttps ? 443 : 80),\n"
    "      path: target.pathname + target.search, method: req.method, headers: hdrs }, up => {\n"
    "      res.writeHead(up.statusCode, {\n"
    "        'Content-Type': up.headers['content-type'] || 'application/json',\n"
    "        'Access-Control-Allow-Origin': '*' });\n"
    "      up.pipe(res);\n"
    "    });\n"
    "    pr.on('error', e => { if (!res.headersSent) { res.writeHead(502); res.end(e.message); } });\n"
    "    if (buf.length) pr.write(buf);\n"
    "    pr.end();\n"
    "  });\n"
    "}\n\n");

  /* ── Request handler + server ── */
  fprintf(out,
    "/* ── HTTP server ────────────────────────────────────────────────────── */\n"
    "const _server = http.createServer(async (req, res) => {\n"
    "  if (req.method === 'OPTIONS') {\n"
    "    res.writeHead(204, { 'Access-Control-Allow-Origin':'*',\n"
    "      'Access-Control-Allow-Methods':'GET,POST,PUT,DELETE,OPTIONS',\n"
    "      'Access-Control-Allow-Headers':'Authorization,Content-Type,Accept' });\n"
    "    res.end(); return;\n"
    "  }\n"
    "  const reqPath = (req.url || '/').split('?')[0];\n"
    "\n"
    "  /* Proxy /api/* → backend */\n"
    "  if (reqPath.startsWith('/api/')) { _proxyApi(req, res); return; }\n"
    "\n"
    "  /* Static assets (have a file extension) */\n"
    "  const ext = path.extname(reqPath);\n"
    "  if (ext) {\n"
    "    try {\n"
    "      const data = fs.readFileSync(path.join(ROOT_DIR, reqPath));\n"
    "      res.writeHead(200, { 'Content-Type': MIME[ext.toLowerCase()] || 'application/octet-stream',\n"
    "                           'Cache-Control': 'public,max-age=300' });\n"
    "      res.end(data);\n"
    "    } catch { res.writeHead(404); res.end('Not found'); }\n"
    "    return;\n"
    "  }\n"
    "\n"
    "  /* SSR for all SPA routes */\n"
    "  let template;\n"
    "  try {\n"
    "    const candidates = ['index.html','base_index.html'].map(n => path.join(ROOT_DIR, n));\n"
    "    const found = candidates.find(p => { try { fs.accessSync(p); return true; } catch { return false; } });\n"
    "    if (!found) throw new Error('No index.html found in ' + ROOT_DIR);\n"
    "    template = fs.readFileSync(found, 'utf8');\n"
    "  } catch (e) {\n"
    "    res.writeHead(500); res.end('Template error: ' + e.message); return;\n"
    "  }\n"
    "\n"
    "  let html = '', meta = { title: 'Forge App', desc: '' }, data = {};\n"
    "  try {\n"
    "    const result = await resolveState(reqPath);\n"
    "    meta  = result.meta  || meta;\n"
    "    data  = result.data  || {};\n"
    "    html  = render(result.state || {}, {});\n"
    "    console.log(`[ssr]  GET ${reqPath}  →  ${html.length} bytes`);\n"
    "  } catch (e) {\n"
    "    console.error('[ssr] resolveState error:', e.message);\n"
    "    /* Fall through — serve static template; client JS still works */\n"
    "  }\n"
    "\n"
    "  const page = _buildPage(template, html, meta, data);\n"
    "  res.writeHead(200, { 'Content-Type':'text/html; charset=utf-8', 'Cache-Control':'no-cache' });\n"
    "  res.end(page);\n"
    "});\n\n");

  /* ── Startup ── */
  fprintf(out,
    "_server.listen(PORT, () => {\n"
    "  console.log('\\n  \\x1b[32mForge SSR Server\\x1b[0m  (<forge-%s>)');\n"
    "  console.log('  \\x1b[36mLocal:\\x1b[0m  http://localhost:' + PORT);\n"
    "  console.log('  \\x1b[36mAPI:\\x1b[0m    ' + API_BASE);\n"
    "  console.log('  \\x1b[33mEdit resolveState() in forge-ssr-server.js to connect your API.\\x1b[0m');\n"
    "  console.log('  Press Ctrl+C to stop\\n');\n"
    "});\n",
    tag);

  return 0;
}
