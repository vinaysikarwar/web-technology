/*
 * Forge Framework - C Code Generator
 *
 * Emits valid C code from the component AST. The generated C is then
 * compiled by Clang to a WASM32 module.
 *
 * Generated output structure for a component named "Button":
 *
 *   typedef struct { ... } Button_Props;
 *   typedef struct { ... } Button_State;
 *
 *   static Button_State __button_state;
 *
 *   // Reactive update functions — called only when a dep changes
 *   void __button_update_clicks(forge_ctx *ctx);
 *
 *   // Event handler functions
 *   void __button_on_click(forge_event *e, forge_ctx *ctx);
 *
 *   // Render: emit DOM diff instructions into ctx->patch_queue
 *   void __button_render(forge_ctx *ctx, const Button_Props *props);
 *
 *   // Lifecycle entry points (exported to WASM host)
 *   FORGE_EXPORT void forge_mount(uint32_t el_id, void *props_json, uint32_t props_len);
 *   FORGE_EXPORT void forge_update(uint32_t el_id, void *props_json, uint32_t props_len);
 *   FORGE_EXPORT void forge_dispatch(uint32_t el_id, forge_event_type ev);
 *   FORGE_EXPORT void forge_unmount(uint32_t el_id);
 */

#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── Helpers ─────────────────────────────────────────────────────────────── */

static void lower(char *dst, const char *src) {
    while (*src) { *dst++ = (char)tolower((unsigned char)*src++); }
    *dst = '\0';
}

/* Emit C type for a TypeRef */
static void emit_type(const TypeRef *t, FILE *out) {
    if (!t) { fprintf(out, "void"); return; }
    switch (t->kind) {
    case TY_INT:      fprintf(out, "int");      break;
    case TY_CHAR:     fprintf(out, "char");     break;
    case TY_BOOL:     fprintf(out, "int");      break; /* C99 _Bool / int */
    case TY_FLOAT:    fprintf(out, "float");    break;
    case TY_DOUBLE:   fprintf(out, "double");   break;
    case TY_VOID:     fprintf(out, "void");     break;
    case TY_LONG:     fprintf(out, "long");     break;
    case TY_SHORT:    fprintf(out, "short");    break;
    case TY_UNSIGNED: fprintf(out, "unsigned"); break;
    case TY_USER:     fprintf(out, "%s", t->name ? t->name : "void"); break;
    case TY_PTR:      emit_type(t->inner, out); fprintf(out, "*"); break;
    case TY_ARRAY:    emit_type(t->inner, out); break; /* name[N] appended later */
    case TY_FN_PTR:
        emit_type(t->ret_type, out);
        fprintf(out, " (*");
        break;
    default: fprintf(out, "void"); break;
    }
}

static void emit_field_decl(const Field *f, FILE *out) {
    emit_type(f->type, out);
    fprintf(out, " %s", f->name);
    if (f->type && f->type->kind == TY_ARRAY) {
        fprintf(out, "[%d]", f->type->array_size > 0 ? f->type->array_size : 64);
    }
    fprintf(out, ";\n");
}

/* ─── Props Struct ────────────────────────────────────────────────────────── */

static void emit_props_struct(const ComponentNode *c, FILE *out) {
    fprintf(out, "/* ── Props ─────────────────────────────── */\n");
    fprintf(out, "typedef struct {\n");
    for (int i = 0; i < c->prop_count; i++) {
        fprintf(out, "    ");
        emit_field_decl(&c->props[i], out);
    }
    if (c->prop_count == 0) fprintf(out, "    char _pad;\n");
    fprintf(out, "} %s_Props;\n\n", c->name);
}

/* ─── State Struct ────────────────────────────────────────────────────────── */

static void emit_state_struct(const ComponentNode *c, FILE *out) {
    fprintf(out, "/* ── State ─────────────────────────────── */\n");
    fprintf(out, "typedef struct {\n");
    for (int i = 0; i < c->state_count; i++) {
        fprintf(out, "    ");
        emit_field_decl(&c->state[i], out);
    }
    if (c->state_count == 0) fprintf(out, "    char _pad;\n");
    fprintf(out, "} %s_State;\n\n", c->name);
}

/* ─── State Initializer ───────────────────────────────────────────────────── */

static void emit_state_init(const ComponentNode *c, FILE *out) {
    char lname[256];
    lower(lname, c->name);

    fprintf(out, "static %s_State __%s_state_init(void) {\n", c->name, lname);
    fprintf(out, "    %s_State __s;\n", c->name);
    fprintf(out, "    forge_memset(&__s, 0, sizeof(__s));\n");
    for (int i = 0; i < c->state_count; i++) {
        if (c->state[i].init_expr) {
            fprintf(out, "    __s.%s = %s;\n",
                    c->state[i].name, c->state[i].init_expr);
        }
    }
    fprintf(out, "    return __s;\n");
    fprintf(out, "}\n\n");
}

/* ─── HTML → DOM Calls ────────────────────────────────────────────────────── */

static int node_counter = 0;

static void emit_html_node(const HtmlNode *n, const char *parent_var,
                            const char *comp_name, FILE *out) {
    if (!n) return;
    int my_id = node_counter++;
    char var[64];
    snprintf(var, sizeof(var), "__el%d", my_id);
    char lname[256];
    lower(lname, comp_name);

    switch (n->kind) {
    case HTML_TEXT:
        if (n->text && n->text[0]) {
            fprintf(out, "    forge_dom_text(%s, \"%s\");\n", parent_var, n->text);
        }
        break;

    case HTML_EXPR:
        /* Expression node: emit a text node updated by a reactive function */
        fprintf(out, "    forge_dom_expr(%s, (forge_expr_fn)__expr_%s_%d, __ctx);\n",
                parent_var, lname, my_id);
        break;

    case HTML_COMPONENT:
        /* Nested component */
        fprintf(out, "    {\n");
        fprintf(out, "        /* mount child component: %s */\n", n->tag ? n->tag : "?");
        fprintf(out, "        forge_dom_node_t *%s = forge_dom_create_component(%s, \"%s\");\n",
                var, parent_var, n->tag ? n->tag : "?");
        for (int i = 0; i < n->attr_count; i++) {
            if (n->attrs[i].is_expr)
                fprintf(out, "        forge_dom_set_prop(%s, \"%s\", (forge_val_t){%s});\n",
                        var, n->attrs[i].name, n->attrs[i].value ? n->attrs[i].value : "0");
            else
                fprintf(out, "        forge_dom_set_prop_str(%s, \"%s\", \"%s\");\n",
                        var, n->attrs[i].name, n->attrs[i].value ? n->attrs[i].value : "");
        }
        fprintf(out, "    }\n");
        break;

    case HTML_ELEMENT:
        fprintf(out, "    forge_dom_node_t *%s = forge_dom_create(%s, \"%s\");\n",
                var, parent_var, n->tag ? n->tag : "div");

        /* Emit attributes */
        for (int i = 0; i < n->attr_count; i++) {
            const char *aname = n->attrs[i].name;
            const char *aval  = n->attrs[i].value ? n->attrs[i].value : "";

            /* Event binding: onclick → forge_dom_on */
            if (strncmp(aname, "on", 2) == 0 && islower((unsigned char)aname[2])) {
                fprintf(out, "    forge_dom_on(%s, \"%s\", __on_%s_%s, __ctx);\n",
                        var, aname + 2, lname, aval);
            } else if (n->attrs[i].is_expr) {
                fprintf(out, "    forge_dom_set_attr_expr(%s, \"%s\", (forge_expr_fn)__attr_%s_%d_%s, __ctx);\n",
                        var, aname, lname, my_id, aname);
            } else {
                fprintf(out, "    forge_dom_set_attr(%s, \"%s\", \"%s\");\n", var, aname, aval);
            }
        }

        /* Recurse children */
        for (int i = 0; i < n->child_count; i++) {
            emit_html_node(&n->children[i], var, comp_name, out);
        }
        break;
    }
}

/* ─── Render Function ─────────────────────────────────────────────────────── */

static void emit_render_fn(const ComponentNode *c, FILE *out) {
    char lname[256];
    lower(lname, c->name);
    node_counter = 0;

    fprintf(out, "/* ── Render ─────────────────────────────── */\n");
    fprintf(out, "static void __%s_render(\n", lname);
    fprintf(out, "        forge_ctx_t *__ctx,\n");
    fprintf(out, "        const %s_Props *props,\n", c->name);
    fprintf(out, "        %s_State *state,\n", c->name);
    fprintf(out, "        forge_dom_node_t *__root) {\n");
    fprintf(out, "    (void)props; (void)state;\n");

    if (c->template_root) {
        emit_html_node(c->template_root, "__root", c->name, out);
    }

    fprintf(out, "}\n\n");
}

/* ─── Event Handlers ──────────────────────────────────────────────────────── */

static void emit_event_handlers(const ComponentNode *c, FILE *out) {
    char lname[256];
    lower(lname, c->name);

    fprintf(out, "/* ── Event Handlers ─────────────────────── */\n");
    for (int i = 0; i < c->handler_count; i++) {
        fprintf(out, "static void __on_%s_%s(\n", lname, c->handlers[i].event_name);
        fprintf(out, "        forge_event_t *event,\n");
        fprintf(out, "        forge_ctx_t   *__ctx) {\n");
        fprintf(out, "    %s_State  *state = (%s_State*)__ctx->state;\n", c->name, c->name);
        fprintf(out, "    const %s_Props *props = (const %s_Props*)__ctx->props;\n", c->name, c->name);
        fprintf(out, "    (void)event; (void)props;\n");
        /* Emit handler body */
        if (c->handlers[i].body) {
            fprintf(out, "    /* user code */\n");
            fprintf(out, "    %s\n", c->handlers[i].body);
        }
        /* After state mutation, trigger reactive update */
        fprintf(out, "    forge_schedule_update(__ctx);\n");
        fprintf(out, "}\n\n");
    }
}

/* ─── Computed Expressions ────────────────────────────────────────────────── */

static void emit_computed(const ComponentNode *c, FILE *out) {
    if (c->computed_count == 0) return;
    char lname[256];
    lower(lname, c->name);

    fprintf(out, "/* ── Computed ───────────────────────────── */\n");
    for (int i = 0; i < c->computed_count; i++) {
        const ComputedField *cf = &c->computed[i];
        fprintf(out, "static forge_val_t __computed_%s_%s(forge_ctx_t *__ctx) {\n",
                lname, cf->field.name);
        fprintf(out, "    %s_State  *state = (%s_State*)__ctx->state;\n", c->name, c->name);
        fprintf(out, "    const %s_Props *props = (const %s_Props*)__ctx->props;\n", c->name, c->name);
        fprintf(out, "    (void)props;\n");
        fprintf(out, "    return forge_val_auto(%s);\n",
                cf->expression ? cf->expression : "0");
        fprintf(out, "}\n\n");
    }
}

/* ─── Lifecycle Exports ───────────────────────────────────────────────────── */

static void emit_lifecycle(const ComponentNode *c, FILE *out) {
    char lname[256];
    lower(lname, c->name);

    fprintf(out, "/* ── Lifecycle Exports ──────────────────── */\n");

    /* forge_mount: called when component is inserted into DOM */
    fprintf(out, "FORGE_EXPORT void forge_mount_%s(\n", lname);
    fprintf(out, "        uint32_t           el_id,\n");
    fprintf(out, "        const uint8_t     *props_json,\n");
    fprintf(out, "        uint32_t           props_len) {\n");
    fprintf(out, "    forge_ctx_t *__ctx = forge_ctx_new(el_id, sizeof(%s_State), sizeof(%s_Props));\n",
            c->name, c->name);
    fprintf(out, "    %s_State *state = (%s_State*)__ctx->state;\n", c->name, c->name);
    fprintf(out, "    *state = __%s_state_init();\n", lname);
    fprintf(out, "    forge_props_deserialize(__ctx->props, props_json, props_len);\n");
    fprintf(out, "    forge_dom_node_t *root = forge_dom_get(el_id);\n");
    fprintf(out, "    __%s_render(__ctx, (%s_Props*)__ctx->props, state, root);\n", lname, c->name);
    fprintf(out, "    forge_ctx_register(__ctx, el_id);\n");
    fprintf(out, "}\n\n");

    /* forge_update: called when props change from parent */
    fprintf(out, "FORGE_EXPORT void forge_update_%s(\n", lname);
    fprintf(out, "        uint32_t           el_id,\n");
    fprintf(out, "        const uint8_t     *props_json,\n");
    fprintf(out, "        uint32_t           props_len) {\n");
    fprintf(out, "    forge_ctx_t *__ctx = forge_ctx_get(el_id);\n");
    fprintf(out, "    if (!__ctx) return;\n");
    fprintf(out, "    forge_props_deserialize(__ctx->props, props_json, props_len);\n");
    fprintf(out, "    forge_schedule_update(__ctx);\n");
    fprintf(out, "}\n\n");

    /* forge_dispatch: route an event to this component */
    fprintf(out, "FORGE_EXPORT void forge_dispatch_%s(\n", lname);
    fprintf(out, "        uint32_t         el_id,\n");
    fprintf(out, "        forge_event_t   *event) {\n");
    fprintf(out, "    forge_ctx_t *__ctx = forge_ctx_get(el_id);\n");
    fprintf(out, "    if (!__ctx) return;\n");
    /* Route to correct handler */
    for (int i = 0; i < c->handler_count; i++) {
        fprintf(out, "    if (forge_event_is(event, \"%s\")) {\n",
                c->handlers[i].event_name);
        fprintf(out, "        __on_%s_%s(event, __ctx);\n", lname, c->handlers[i].event_name);
        fprintf(out, "        return;\n");
        fprintf(out, "    }\n");
    }
    fprintf(out, "}\n\n");

    /* forge_unmount: cleanup */
    fprintf(out, "FORGE_EXPORT void forge_unmount_%s(uint32_t el_id) {\n", lname);
    fprintf(out, "    forge_ctx_t *__ctx = forge_ctx_get(el_id);\n");
    fprintf(out, "    if (__ctx) { forge_ctx_free(__ctx); forge_ctx_unregister(el_id); }\n");
    fprintf(out, "}\n\n");
}

/* ─── CSS Class Generation ────────────────────────────────────────────────── */

static void emit_styles(const ComponentNode *c, FILE *out) {
    if (c->style_count == 0) return;
    char lname[256];
    lower(lname, c->name);

    fprintf(out, "/* ── Static Styles (injected at mount) ──── */\n");
    fprintf(out, "static const char *__%s_css =\n", lname);
    fprintf(out, "    \"[data-forge-%s] {\\n\"\n", lname);
    for (int i = 0; i < c->style_count; i++) {
        if (!c->style[i].is_dynamic) {
            fprintf(out, "    \"    %s: %s;\\n\"\n",
                    c->style[i].property, c->style[i].value);
        }
    }
    fprintf(out, "    \"}\\n\";\n\n");

    /* Dynamic style updater */
    int has_dynamic = 0;
    for (int i = 0; i < c->style_count; i++) if (c->style[i].is_dynamic) { has_dynamic = 1; break; }

    if (has_dynamic) {
        fprintf(out, "static void __%s_update_styles(\n", lname);
        fprintf(out, "        forge_dom_node_t *el,\n");
        fprintf(out, "        const %s_Props *props,\n", c->name);
        fprintf(out, "        const %s_State *state) {\n", c->name);
        fprintf(out, "    (void)props; (void)state;\n");
        for (int i = 0; i < c->style_count; i++) {
            if (c->style[i].is_dynamic) {
                fprintf(out, "    forge_dom_set_style(el, \"%s\", (forge_expr_fn)0, \"%s\");\n",
                        c->style[i].property, c->style[i].value);
            }
        }
        fprintf(out, "}\n\n");
    }
}

/* ─── File Header ─────────────────────────────────────────────────────────── */

static void emit_file_header(const ComponentNode *c, FILE *out) {
    fprintf(out,
        "/*\n"
        " * AUTO-GENERATED by Forge Compiler\n"
        " * Component: %s\n"
        " * DO NOT EDIT — regenerate with: forge compile %s.cx\n"
        " */\n\n"
        "#include <forge/runtime.h>\n"
        "#include <forge/dom.h>\n"
        "#include <stdint.h>\n\n",
        c->name, c->name);
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

int codegen_component(const ComponentNode *c, const CodegenOptions *opts, FILE *out) {
    (void)opts;

    emit_file_header(c, out);
    emit_props_struct(c, out);
    emit_state_struct(c, out);
    emit_state_init(c, out);
    emit_styles(c, out);
    emit_computed(c, out);
    emit_event_handlers(c, out);
    emit_render_fn(c, out);
    emit_lifecycle(c, out);

    return 0;
}

int codegen_program(const Program *p, const CodegenOptions *opts, const char *out_dir) {
    int rc = 0;
    for (int i = 0; i < p->component_count; i++) {
        const ComponentNode *c = p->components[i];
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.gen.c", out_dir, c->name);

        FILE *f = fopen(path, "w");
        if (!f) {
            fprintf(stderr, "forge: cannot open output file '%s'\n", path);
            rc = 1;
            continue;
        }
        rc |= codegen_component(c, opts, f);
        fclose(f);
        printf("forge: generated %s\n", path);
    }
    return rc;
}
