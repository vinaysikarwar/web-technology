/*
 * Forge Framework - Parser Implementation
 *
 * Recursive-descent parser. Produces a ComponentNode AST for each
 * @component block found in the source file.
 */

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Internals ───────────────────────────────────────────────────────────── */

static int  p_error_count = 0;

static void error_at(Parser *p, const Token *tok, const char *msg) {
    if (p->panic_mode) return;
    p->panic_mode = 1;
    p->had_error  = 1;
    p_error_count++;
    fprintf(stderr, "\033[31m[forge] ERROR\033[0m %s:%d:%d  %s\n",
            tok->loc.filename, tok->loc.line, tok->loc.column, msg);
}

static void advance(Parser *p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next(p->lex);
        if (p->current.type != TOK_ERROR) break;
        error_at(p, &p->current, p->current.start);
    }
}

static void consume(Parser *p, TokenType type, const char *err) {
    if (p->current.type == type) { advance(p); return; }
    error_at(p, &p->current, err);
}

static int check(Parser *p, TokenType type) { return p->current.type == type; }
static int match_tok(Parser *p, TokenType type) {
    if (!check(p, type)) return 0;
    advance(p); return 1;
}

/* Make a null-terminated copy of a token's text */
static char *tok_dup(const Token *tok) {
    char *s = malloc(tok->length + 1);
    memcpy(s, tok->start, tok->length);
    s[tok->length] = '\0';
    return s;
}

/* ─── Type Parsing ────────────────────────────────────────────────────────── */

static TypeRef *parse_type(Parser *p) {
    TypeRef *tr = ast_new_type(TY_USER);

    /* const qualifier */
    if (match_tok(p, TOK_CONST)) tr->is_const = 1;

    /* Base type */
    switch (p->current.type) {
    case TOK_INT:      tr->kind = TY_INT;      advance(p); break;
    case TOK_CHAR:     tr->kind = TY_CHAR;     advance(p); break;
    case TOK_BOOL:     tr->kind = TY_BOOL;     advance(p); break;
    case TOK_FLOAT:    tr->kind = TY_FLOAT;    advance(p); break;
    case TOK_DOUBLE:   tr->kind = TY_DOUBLE;   advance(p); break;
    case TOK_VOID:     tr->kind = TY_VOID;     advance(p); break;
    case TOK_LONG:     tr->kind = TY_LONG;     advance(p); break;
    case TOK_SHORT:    tr->kind = TY_SHORT;    advance(p); break;
    case TOK_UNSIGNED: tr->kind = TY_UNSIGNED; advance(p); break;
    case TOK_IDENT:
        tr->kind = TY_USER;
        tr->name = tok_dup(&p->current);
        advance(p);
        break;
    default:
        error_at(p, &p->current, "Expected type name");
        break;
    }

    /* Pointer suffix: char* */
    while (match_tok(p, TOK_STAR)) {
        TypeRef *ptr = ast_new_type(TY_PTR);
        ptr->inner = tr;
        tr = ptr;
    }

    /* Function pointer: void (*name)(args) — detect by peeking */
    /* Array suffix: type name[N] — handled in field parsing */

    return tr;
}

/* ─── Raw Block Capture ───────────────────────────────────────────────────── */

/* Capture everything between { } as a raw string (for event handler bodies) */
static char *capture_block(Parser *p) {
    consume(p, TOK_LBRACE, "Expected '{' to open block");
    const char *start = p->lex->current;
    int depth = 1;
    while (peek_char_raw(p->lex) && depth > 0) {
        char c = *p->lex->current;
        if (c == '{') depth++;
        if (c == '}') depth--;
        if (depth > 0) {
            /* advance raw pointer */
            if (c == '\n') { p->lex->line++; p->lex->line_start = p->lex->current + 1; }
            p->lex->current++;
        }
    }
    size_t len = (size_t)(p->lex->current - start);
    char *body = malloc(len + 1);
    memcpy(body, start, len);
    body[len] = '\0';
    /* consume closing } */
    if (*p->lex->current == '}') {
        p->lex->current++;
    }
    /* Re-sync parser token */
    advance(p);
    return body;
}

/* Allow raw character peeking through lexer struct */
static char peek_char_raw(Lexer *lex) { return *lex->current; }

/* ─── Field Parsing (@props, @state sections) ─────────────────────────────── */

static Field parse_field(Parser *p) {
    Field f = ast_new_field();
    f.type = parse_type(p);

    /* Capture name */
    if (!check(p, TOK_IDENT)) {
        error_at(p, &p->current, "Expected field name");
        return f;
    }
    f.name = tok_dup(&p->current);
    advance(p);

    /* Array dimension: name[N] */
    if (match_tok(p, TOK_LBRACKET)) {
        TypeRef *arr = ast_new_type(TY_ARRAY);
        arr->inner = f.type;
        arr->array_size = -1;
        if (check(p, TOK_INT_LIT)) {
            arr->array_size = (int)p->current.value.int_val;
            advance(p);
        }
        consume(p, TOK_RBRACKET, "Expected ']'");
        f.type = arr;
    }

    /* Optional initializer: = expr */
    if (match_tok(p, TOK_ASSIGN)) {
        const char *init_start = p->lex->current;
        /* collect everything up to ; */
        while (!check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) advance(p);
        size_t len = (size_t)(p->current.start - init_start);
        f.init_expr = malloc(len + 1);
        memcpy(f.init_expr, init_start, len);
        f.init_expr[len] = '\0';
    }

    consume(p, TOK_SEMICOLON, "Expected ';' after field declaration");
    return f;
}

/* ─── Style Section ───────────────────────────────────────────────────────── */

static void parse_style_section(Parser *p, ComponentNode *comp) {
    /* Already consumed @style, now read { property: value; ... } */
    lexer_set_mode(p->lex, LEX_MODE_STYLE);
    advance(p); /* sync current token */

    consume(p, TOK_LBRACE, "Expected '{' after @style");

    int cap = 8;
    comp->style = malloc(sizeof(StyleRule) * cap);
    comp->style_count = 0;

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        StyleRule rule;
        rule.is_dynamic = 0;

        /* property name */
        if (!check(p, TOK_HTML_ATTR) && !check(p, TOK_IDENT)) break;
        rule.property = tok_dup(&p->current);
        advance(p);

        consume(p, TOK_COLON, "Expected ':' after style property");

        /* value (up to ;) */
        const char *val_start = p->current.start;
        while (!check(p, TOK_SEMICOLON) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF))
            advance(p);
        size_t vlen = (size_t)(p->current.start - val_start);
        rule.value = malloc(vlen + 1);
        memcpy(rule.value, val_start, vlen);
        /* trim trailing whitespace */
        while (vlen > 0 && (rule.value[vlen-1] == ' ' || rule.value[vlen-1] == '\t')) vlen--;
        rule.value[vlen] = '\0';

        /* detect dynamic value (contains 'props.' or 'state.') */
        if (strstr(rule.value, "props.") || strstr(rule.value, "state."))
            rule.is_dynamic = 1;

        match_tok(p, TOK_SEMICOLON);

        if (comp->style_count >= cap) { cap *= 2; comp->style = realloc(comp->style, sizeof(StyleRule) * cap); }
        comp->style[comp->style_count++] = rule;
    }

    consume(p, TOK_RBRACE, "Expected '}' to close @style");
    lexer_set_mode(p->lex, LEX_MODE_C);
}

/* ─── Template Parsing ────────────────────────────────────────────────────── */

static HtmlNode *parse_html_node(Parser *p);

static char *collect_expr(Parser *p) {
    /* cursor is right after { — collect until matching } */
    const char *start = p->lex->current;
    int depth = 1;
    while (*p->lex->current && depth > 0) {
        if (*p->lex->current == '{') depth++;
        if (*p->lex->current == '}') depth--;
        if (depth > 0) p->lex->current++;
    }
    size_t len = (size_t)(p->lex->current - start);
    char *expr = malloc(len + 1);
    memcpy(expr, start, len);
    expr[len] = '\0';
    if (*p->lex->current == '}') p->lex->current++; /* consume } */
    advance(p); /* re-sync */
    return expr;
}

static HtmlNode *parse_element(Parser *p, const char *tag) {
    HtmlNode *node = ast_new_html_node(HTML_ELEMENT);
    node->tag = strdup(tag);

    /* Check if it's a component (starts with uppercase) */
    if (tag[0] >= 'A' && tag[0] <= 'Z') node->kind = HTML_COMPONENT;

    /* Parse attributes */
    int attr_cap = 4;
    node->attrs = malloc(sizeof(Attribute) * attr_cap);
    node->attr_count = 0;

    while (!check(p, TOK_GT) && !check(p, TOK_SLASH) && !check(p, TOK_EOF)) {
        if (!check(p, TOK_IDENT) && !check(p, TOK_HTML_ATTR)) break;

        Attribute attr;
        attr.name    = tok_dup(&p->current);
        attr.value   = NULL;
        attr.is_expr = 0;
        advance(p);

        if (match_tok(p, TOK_ASSIGN)) {
            if (match_tok(p, TOK_LBRACE)) {
                attr.value   = collect_expr(p);
                attr.is_expr = 1;
            } else if (check(p, TOK_STRING_LIT)) {
                attr.value   = strdup(p->current.value.str_val);
                advance(p);
            } else {
                attr.value = tok_dup(&p->current);
                advance(p);
            }
        }

        if (node->attr_count >= attr_cap) {
            attr_cap *= 2;
            node->attrs = realloc(node->attrs, sizeof(Attribute) * attr_cap);
        }
        node->attrs[node->attr_count++] = attr;
    }

    /* Self-closing <Tag /> */
    if (match_tok(p, TOK_SLASH)) {
        consume(p, TOK_GT, "Expected '>' after '/'");
        node->self_closing = 1;
        return node;
    }
    consume(p, TOK_GT, "Expected '>' after tag attributes");

    /* Parse children */
    int child_cap = 4;
    node->children = malloc(sizeof(HtmlNode) * child_cap);
    node->child_count = 0;

    while (!check(p, TOK_EOF)) {
        /* Check for closing tag </tag> */
        if (check(p, TOK_LT)) {
            Token saved = p->current;
            advance(p); /* consume < */
            if (match_tok(p, TOK_SLASH)) {
                /* closing tag */
                advance(p); /* tag name */
                consume(p, TOK_GT, "Expected '>' in closing tag");
                break;
            }
            /* not a closing tag — put back and parse child */
            /* We need to parse the child element starting with the tag name */
            if (!check(p, TOK_IDENT)) break;
            char *child_tag = tok_dup(&p->current);
            advance(p);
            HtmlNode *child = parse_element(p, child_tag);
            free(child_tag);
            if (node->child_count >= child_cap) {
                child_cap *= 2;
                node->children = realloc(node->children, sizeof(HtmlNode) * child_cap);
            }
            node->children[node->child_count++] = *child;
            free(child);
            continue;
        }

        /* Expression node {expr} */
        if (match_tok(p, TOK_LBRACE)) {
            HtmlNode *expr_node = ast_new_html_node(HTML_EXPR);
            expr_node->text = collect_expr(p);
            if (node->child_count >= child_cap) {
                child_cap *= 2;
                node->children = realloc(node->children, sizeof(HtmlNode) * child_cap);
            }
            node->children[node->child_count++] = *expr_node;
            free(expr_node);
            continue;
        }

        /* Text node */
        if (check(p, TOK_HTML_TEXT)) {
            HtmlNode *text_node = ast_new_html_node(HTML_TEXT);
            text_node->text = tok_dup(&p->current);
            advance(p);
            if (node->child_count >= child_cap) {
                child_cap *= 2;
                node->children = realloc(node->children, sizeof(HtmlNode) * child_cap);
            }
            node->children[node->child_count++] = *text_node;
            free(text_node);
            continue;
        }

        break;
    }

    return node;
}

static void parse_template_section(Parser *p, ComponentNode *comp) {
    lexer_set_mode(p->lex, LEX_MODE_TEMPLATE);
    advance(p);

    consume(p, TOK_LBRACE, "Expected '{' after @template");

    /* Parse the root element */
    /* skip whitespace tokens */
    while (check(p, TOK_HTML_TEXT)) {
        /* check if pure whitespace */
        int all_ws = 1;
        for (size_t i = 0; i < p->current.length; i++) {
            if (!isspace((unsigned char)p->current.start[i])) { all_ws = 0; break; }
        }
        if (!all_ws) break;
        advance(p);
    }

    if (check(p, TOK_LT)) {
        advance(p); /* consume < */
        if (check(p, TOK_IDENT)) {
            char *tag = tok_dup(&p->current);
            advance(p);
            comp->template_root = parse_element(p, tag);
            free(tag);
        }
    }

    /* consume closing } of @template block */
    match_tok(p, TOK_RBRACE);
    lexer_set_mode(p->lex, LEX_MODE_C);
    advance(p);
}

/* ─── Component Parsing ───────────────────────────────────────────────────── */

static ComponentNode *parse_component(Parser *p) {
    /* @component consumed — read name */
    if (!check(p, TOK_IDENT)) {
        error_at(p, &p->current, "Expected component name after @component");
        return NULL;
    }
    ComponentNode *comp = ast_new_component();
    comp->name = tok_dup(&p->current);
    comp->loc  = p->current.loc;
    advance(p);

    consume(p, TOK_LBRACE, "Expected '{' to open @component body");

    int prop_cap    = 8, state_cap = 8,
        handler_cap = 4, computed_cap = 4;

    comp->props    = malloc(sizeof(Field) * prop_cap);
    comp->state    = malloc(sizeof(Field) * state_cap);
    comp->handlers = malloc(sizeof(EventHandler) * handler_cap);
    comp->computed = malloc(sizeof(ComputedField) * computed_cap);

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {

        /* @props { ... } */
        if (match_tok(p, TOK_AT_PROPS)) {
            consume(p, TOK_LBRACE, "Expected '{' after @props");
            while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                if (comp->prop_count >= prop_cap) { prop_cap *= 2; comp->props = realloc(comp->props, sizeof(Field) * prop_cap); }
                comp->props[comp->prop_count++] = parse_field(p);
                p->panic_mode = 0;
            }
            consume(p, TOK_RBRACE, "Expected '}' to close @props");
            continue;
        }

        /* @state { ... } */
        if (match_tok(p, TOK_AT_STATE)) {
            consume(p, TOK_LBRACE, "Expected '{' after @state");
            while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                if (comp->state_count >= state_cap) { state_cap *= 2; comp->state = realloc(comp->state, sizeof(Field) * state_cap); }
                comp->state[comp->state_count++] = parse_field(p);
                p->panic_mode = 0;
            }
            consume(p, TOK_RBRACE, "Expected '}' to close @state");
            continue;
        }

        /* @style { ... } */
        if (match_tok(p, TOK_AT_STYLE)) {
            parse_style_section(p, comp);
            continue;
        }

        /* @on(eventName) { ... } */
        if (match_tok(p, TOK_AT_ON)) {
            consume(p, TOK_LPAREN, "Expected '(' after @on");
            if (!check(p, TOK_IDENT)) { error_at(p, &p->current, "Expected event name"); break; }
            EventHandler ev;
            ev.event_name = tok_dup(&p->current);
            advance(p);
            consume(p, TOK_RPAREN, "Expected ')' after event name");

            const char *body_start = p->lex->current;
            consume(p, TOK_LBRACE, "Expected '{' for event handler body");
            int depth = 1;
            while (*p->lex->current && depth > 0) {
                if (*p->lex->current == '{') depth++;
                if (*p->lex->current == '}') depth--;
                if (depth > 0) {
                    if (*p->lex->current == '\n') { p->lex->line++; p->lex->line_start = p->lex->current + 1; }
                    p->lex->current++;
                }
            }
            size_t blen = (size_t)(p->lex->current - body_start - 1);
            ev.body = malloc(blen + 1);
            memcpy(ev.body, body_start + 1, blen); /* skip opening { */
            ev.body[blen] = '\0';
            if (*p->lex->current == '}') p->lex->current++;

            if (comp->handler_count >= handler_cap) { handler_cap *= 2; comp->handlers = realloc(comp->handlers, sizeof(EventHandler) * handler_cap); }
            comp->handlers[comp->handler_count++] = ev;
            advance(p);
            continue;
        }

        /* @computed { ... } */
        if (match_tok(p, TOK_AT_COMPUTED)) {
            consume(p, TOK_LBRACE, "Expected '{' after @computed");
            while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                ComputedField cf;
                cf.field = parse_field(p);
                /* the init_expr IS the computed expression */
                cf.expression = cf.field.init_expr;
                cf.field.init_expr = NULL;
                if (comp->computed_count >= computed_cap) { computed_cap *= 2; comp->computed = realloc(comp->computed, sizeof(ComputedField) * computed_cap); }
                comp->computed[comp->computed_count++] = cf;
                p->panic_mode = 0;
            }
            consume(p, TOK_RBRACE, "Expected '}' to close @computed");
            continue;
        }

        /* @template { ... } */
        if (match_tok(p, TOK_AT_TEMPLATE)) {
            parse_template_section(p, comp);
            continue;
        }

        /* Unknown token — skip with error */
        error_at(p, &p->current, "Unexpected token in component body");
        advance(p);
        p->panic_mode = 0;
    }

    consume(p, TOK_RBRACE, "Expected '}' to close @component");
    return comp;
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

void parser_init(Parser *p, Lexer *lex) {
    p->lex        = lex;
    p->had_error  = 0;
    p->panic_mode = 0;
    p_error_count = 0;
    advance(p); /* prime the pump */
}

Program *parser_parse(Parser *p) {
    Program *prog = malloc(sizeof(Program));
    prog->components      = NULL;
    prog->component_count = 0;
    int cap = 4;
    prog->components = malloc(sizeof(ComponentNode *) * cap);

    while (!check(p, TOK_EOF)) {
        /* #include "..." */
        if (match_tok(p, TOK_HASH) || match_tok(p, TOK_INCLUDE)) {
            /* skip include line */
            while (!check(p, TOK_EOF) &&
                   p->current.loc.line == p->previous.loc.line) advance(p);
            continue;
        }

        /* @component Name { ... } */
        if (match_tok(p, TOK_AT_COMPONENT)) {
            ComponentNode *comp = parse_component(p);
            if (comp) {
                if (prog->component_count >= cap) { cap *= 2; prog->components = realloc(prog->components, sizeof(ComponentNode *) * cap); }
                prog->components[prog->component_count++] = comp;
            }
            p->panic_mode = 0;
            continue;
        }

        /* Unknown top-level token */
        if (!check(p, TOK_EOF)) {
            error_at(p, &p->current, "Expected @component at top level");
            advance(p);
            p->panic_mode = 0;
        }
    }

    return prog;
}

void parser_free(Parser *p) { (void)p; }
int  parser_error_count(Parser *p) { (void)p; return p_error_count; }
