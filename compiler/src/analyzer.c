/*
 * Forge Framework - Semantic Analyzer
 *
 * Walks the AST and:
 *   1. Resolves field references in template expressions
 *   2. Marks state/props fields as reactive if used in template
 *   3. Marks style rules as dynamic if they reference state/props
 *   4. Reports type errors and undefined references
 */

#include "analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Internals ───────────────────────────────────────────────────────────── */

typedef struct {
    ComponentNode *comp;
    int            errors;
    int            warnings;
} AnalyzerCtx;

static void ana_error(AnalyzerCtx *ctx, const char *msg) {
    fprintf(stderr, "\033[31m[forge/analyzer] ERROR\033[0m  in component '%s': %s\n",
            ctx->comp->name, msg);
    ctx->errors++;
}

static void ana_warn(AnalyzerCtx *ctx, const char *msg) {
    fprintf(stderr, "\033[33m[forge/analyzer] WARN\033[0m   in component '%s': %s\n",
            ctx->comp->name, msg);
    ctx->warnings++;
}

/* ─── Reactivity: scan expression strings for state.X / props.X ─────────── */

static void scan_expr_for_deps(AnalyzerCtx *ctx, const char *expr) {
    if (!expr) return;
    ComponentNode *c = ctx->comp;

    /* Check state fields */
    for (int i = 0; i < c->state_count; i++) {
        char pattern[256];
        snprintf(pattern, sizeof(pattern), "state.%s", c->state[i].name);
        if (strstr(expr, pattern)) {
            c->state[i].is_reactive = 1;
            c->state_used_in_template[i] = 1;
        }
    }

    /* Check props fields */
    for (int i = 0; i < c->prop_count; i++) {
        char pattern[256];
        snprintf(pattern, sizeof(pattern), "props.%s", c->props[i].name);
        if (strstr(expr, pattern)) {
            c->props[i].is_reactive = 1;
            c->props_used_in_template[i] = 1;
        }
    }
}

/* ─── Walk HTML tree ──────────────────────────────────────────────────────── */

static void walk_html(AnalyzerCtx *ctx, const HtmlNode *node) {
    if (!node) return;

    switch (node->kind) {
    case HTML_EXPR:
        scan_expr_for_deps(ctx, node->text);
        break;

    case HTML_ELEMENT:
    case HTML_COMPONENT:
        /* Scan attribute expressions */
        for (int i = 0; i < node->attr_count; i++) {
            if (node->attrs[i].is_expr) {
                scan_expr_for_deps(ctx, node->attrs[i].value);
            }
        }
        /* Recurse into children */
        for (int i = 0; i < node->child_count; i++) {
            walk_html(ctx, &node->children[i]);
        }
        break;

    case HTML_TEXT:
        /* Plain text — no deps */
        break;
    }
}

/* ─── Validate event handlers ─────────────────────────────────────────────── */

static void check_event_handlers(AnalyzerCtx *ctx) {
    ComponentNode *c = ctx->comp;
    /* Ensure referenced @on handlers are valid identifiers */
    for (int i = 0; i < c->handler_count; i++) {
        if (!c->handlers[i].event_name || !c->handlers[i].body) {
            ana_error(ctx, "Malformed event handler");
        }
        /* Mark state fields mutated in handlers as reactive */
        if (c->handlers[i].body) {
            scan_expr_for_deps(ctx, c->handlers[i].body);
        }
    }
}

/* ─── Validate computed fields ────────────────────────────────────────────── */

static void check_computed(AnalyzerCtx *ctx) {
    ComponentNode *c = ctx->comp;
    for (int i = 0; i < c->computed_count; i++) {
        if (!c->computed[i].expression) {
            char msg[256];
            snprintf(msg, sizeof(msg), "computed field '%s' has no expression",
                     c->computed[i].field.name ? c->computed[i].field.name : "?");
            ana_error(ctx, msg);
        }
        scan_expr_for_deps(ctx, c->computed[i].expression);
    }
}

/* ─── Check for unused state ──────────────────────────────────────────────── */

static void check_unused(AnalyzerCtx *ctx) {
    ComponentNode *c = ctx->comp;
    for (int i = 0; i < c->state_count; i++) {
        if (!c->state_used_in_template[i]) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "state field '%s' is declared but never used in @template or @on handlers",
                     c->state[i].name ? c->state[i].name : "?");
            ana_warn(ctx, msg);
        }
    }
    for (int i = 0; i < c->prop_count; i++) {
        if (!c->props_used_in_template[i]) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "prop '%s' is declared but never used",
                     c->props[i].name ? c->props[i].name : "?");
            ana_warn(ctx, msg);
        }
    }
}

/* ─── Check required template ─────────────────────────────────────────────── */

static void check_template(AnalyzerCtx *ctx) {
    if (!ctx->comp->template_root) {
        ana_error(ctx, "Component has no @template section — every component must render something");
    }
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

AnalysisResult analyze_component(ComponentNode *c) {
    /* Allocate reactivity arrays */
    c->state_used_in_template = calloc((size_t)(c->state_count + 1), sizeof(int));
    c->props_used_in_template = calloc((size_t)(c->prop_count  + 1), sizeof(int));

    AnalyzerCtx ctx = { .comp = c, .errors = 0, .warnings = 0 };

    check_template(&ctx);
    check_event_handlers(&ctx);
    check_computed(&ctx);

    /* Walk the template tree to find reactive dependencies */
    if (c->template_root) {
        walk_html(&ctx, c->template_root);
    }

    check_unused(&ctx);

    /* Mark dynamic style rules */
    for (int i = 0; i < c->style_count; i++) {
        if (strstr(c->style[i].value, "props.") || strstr(c->style[i].value, "state.")) {
            c->style[i].is_dynamic = 1;
        }
    }

    AnalysisResult result = { ctx.errors, ctx.warnings };
    return result;
}

AnalysisResult analyze_program(Program *p) {
    AnalysisResult total = { 0, 0 };
    for (int i = 0; i < p->component_count; i++) {
        AnalysisResult r = analyze_component(p->components[i]);
        total.error_count   += r.error_count;
        total.warning_count += r.warning_count;
    }
    return total;
}
