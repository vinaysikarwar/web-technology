/*
 * Forge Framework - AST Helpers
 * Allocation, initialization, and debug dump for AST nodes.
 */

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Allocators ──────────────────────────────────────────────────────────── */

ComponentNode *ast_new_component(void) {
    ComponentNode *c = calloc(1, sizeof(ComponentNode));
    return c;
}

HtmlNode *ast_new_html_node(HtmlKind kind) {
    HtmlNode *n = calloc(1, sizeof(HtmlNode));
    n->kind = kind;
    return n;
}

TypeRef *ast_new_type(TypeKind kind) {
    TypeRef *t = calloc(1, sizeof(TypeRef));
    t->kind       = kind;
    t->array_size = -1;
    return t;
}

Field ast_new_field(void) {
    Field f;
    memset(&f, 0, sizeof(f));
    return f;
}

/* ─── Freeing ─────────────────────────────────────────────────────────────── */

static void free_type(TypeRef *t) {
    if (!t) return;
    free(t->name);
    free_type(t->inner);
    free_type(t->ret_type);
    for (int i = 0; i < t->param_count; i++) free_type(t->param_types[i]);
    free(t->param_types);
    free(t);
}

static void free_field(Field *f) {
    free(f->name);
    free_type(f->type);
    free(f->init_expr);
}

static void free_html(HtmlNode *n, int count) {
    if (!n) return;
    for (int i = 0; i < count; i++) {
        free(n[i].tag);
        free(n[i].text);
        for (int j = 0; j < n[i].attr_count; j++) {
            free(n[i].attrs[j].name);
            free(n[i].attrs[j].value);
        }
        free(n[i].attrs);
        free_html(n[i].children, n[i].child_count);
    }
}

void ast_free_component(ComponentNode *c) {
    if (!c) return;
    free(c->name);
    for (int i = 0; i < c->prop_count;     i++) free_field(&c->props[i]);
    for (int i = 0; i < c->state_count;    i++) free_field(&c->state[i]);
    for (int i = 0; i < c->style_count;    i++) { free(c->style[i].property); free(c->style[i].value); }
    for (int i = 0; i < c->handler_count;  i++) { free(c->handlers[i].event_name); free(c->handlers[i].body); }
    for (int i = 0; i < c->computed_count; i++) { free_field(&c->computed[i].field); free(c->computed[i].expression); }
    for (int i = 0; i < c->include_count;  i++) free(c->includes[i]);
    free(c->props);   free(c->state);    free(c->style);
    free(c->handlers); free(c->computed); free(c->includes);
    free(c->state_used_in_template);
    free(c->props_used_in_template);
    if (c->template_root) {
        free_html(c->template_root, 1);
        free(c->template_root);
    }
    free(c);
}

void ast_free_program(Program *p) {
    if (!p) return;
    for (int i = 0; i < p->component_count; i++) ast_free_component(p->components[i]);
    free(p->components);
    free(p);
}

/* ─── Debug Dump ──────────────────────────────────────────────────────────── */

static const char *type_kind_name(TypeKind k) {
    switch (k) {
    case TY_INT:      return "int";
    case TY_CHAR:     return "char";
    case TY_BOOL:     return "bool";
    case TY_FLOAT:    return "float";
    case TY_DOUBLE:   return "double";
    case TY_VOID:     return "void";
    case TY_LONG:     return "long";
    case TY_SHORT:    return "short";
    case TY_UNSIGNED: return "unsigned";
    case TY_STRUCT:   return "struct";
    case TY_ENUM:     return "enum";
    case TY_USER:     return "user";
    case TY_PTR:      return "ptr";
    case TY_ARRAY:    return "array";
    case TY_FN_PTR:   return "fn_ptr";
    default:          return "?";
    }
}

static void dump_type(const TypeRef *t) {
    if (!t) { printf("(null)"); return; }
    if (t->kind == TY_PTR)   { dump_type(t->inner); printf("*"); return; }
    if (t->kind == TY_ARRAY) { dump_type(t->inner); printf("[%d]", t->array_size); return; }
    if (t->kind == TY_USER)  { printf("%s", t->name ? t->name : "?"); return; }
    printf("%s", type_kind_name(t->kind));
}

static void indent(int n) { for (int i = 0; i < n * 2; i++) putchar(' '); }

static void dump_html(const HtmlNode *n, int depth) {
    if (!n) return;
    indent(depth);
    switch (n->kind) {
    case HTML_TEXT:    printf("TEXT: \"%s\"\n", n->text ? n->text : ""); break;
    case HTML_EXPR:    printf("EXPR: {%s}\n",   n->text ? n->text : ""); break;
    case HTML_ELEMENT:
    case HTML_COMPONENT:
        printf("<%s", n->tag ? n->tag : "?");
        for (int i = 0; i < n->attr_count; i++) {
            if (n->attrs[i].is_expr)
                printf(" %s={%s}", n->attrs[i].name, n->attrs[i].value ? n->attrs[i].value : "");
            else
                printf(" %s=\"%s\"", n->attrs[i].name, n->attrs[i].value ? n->attrs[i].value : "");
        }
        if (n->self_closing) { printf(" />\n"); break; }
        printf(">\n");
        for (int i = 0; i < n->child_count; i++) dump_html(&n->children[i], depth + 1);
        indent(depth);
        printf("</%s>\n", n->tag ? n->tag : "?");
        break;
    }
}

void ast_dump_component(const ComponentNode *c, int depth) {
    indent(depth); printf("@component %s {\n", c->name ? c->name : "?");

    if (c->prop_count > 0) {
        indent(depth + 1); printf("@props {\n");
        for (int i = 0; i < c->prop_count; i++) {
            indent(depth + 2);
            dump_type(c->props[i].type);
            printf(" %s", c->props[i].name ? c->props[i].name : "?");
            if (c->props[i].init_expr) printf(" = %s", c->props[i].init_expr);
            printf(";\n");
        }
        indent(depth + 1); printf("}\n");
    }

    if (c->state_count > 0) {
        indent(depth + 1); printf("@state {\n");
        for (int i = 0; i < c->state_count; i++) {
            indent(depth + 2);
            dump_type(c->state[i].type);
            printf(" %s", c->state[i].name ? c->state[i].name : "?");
            if (c->state[i].init_expr) printf(" = %s", c->state[i].init_expr);
            printf("; [reactive=%d]\n", c->state[i].is_reactive);
        }
        indent(depth + 1); printf("}\n");
    }

    if (c->style_count > 0) {
        indent(depth + 1); printf("@style {\n");
        for (int i = 0; i < c->style_count; i++) {
            indent(depth + 2);
            printf("%s: %s; [dynamic=%d]\n",
                   c->style[i].property, c->style[i].value, c->style[i].is_dynamic);
        }
        indent(depth + 1); printf("}\n");
    }

    for (int i = 0; i < c->handler_count; i++) {
        indent(depth + 1);
        printf("@on(%s) { ... }\n", c->handlers[i].event_name);
    }

    if (c->template_root) {
        indent(depth + 1); printf("@template {\n");
        dump_html(c->template_root, depth + 2);
        indent(depth + 1); printf("}\n");
    }

    indent(depth); printf("}\n");
}

void ast_dump_program(const Program *p) {
    printf("=== Forge AST Dump ===\n");
    for (int i = 0; i < p->component_count; i++) {
        ast_dump_component(p->components[i], 0);
        printf("\n");
    }
}
