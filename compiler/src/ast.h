/*
 * Forge Framework - Abstract Syntax Tree
 *
 * All node types produced by the parser from a .cx component file.
 */

#ifndef FORGE_AST_H
#define FORGE_AST_H

#include <stddef.h>
#include <stdint.h>
#include "lexer.h"

/* ─── Forward Declarations ────────────────────────────────────────────────── */

typedef struct AstNode    AstNode;
typedef struct TypeRef    TypeRef;
typedef struct Field      Field;
typedef struct StyleRule  StyleRule;
typedef struct HtmlNode   HtmlNode;
typedef struct Attribute  Attribute;

/* ─── Type Reference ──────────────────────────────────────────────────────── */

typedef enum {
    TY_INT, TY_CHAR, TY_BOOL, TY_FLOAT, TY_DOUBLE, TY_VOID,
    TY_LONG, TY_SHORT, TY_UNSIGNED, TY_STRUCT, TY_ENUM,
    TY_USER,    /* user-defined type name */
    TY_PTR,     /* pointer to another type */
    TY_ARRAY,   /* array type */
    TY_FN_PTR   /* function pointer */
} TypeKind;

struct TypeRef {
    TypeKind    kind;
    char       *name;       /* for TY_USER / TY_STRUCT */
    TypeRef    *inner;      /* for TY_PTR / TY_ARRAY   */
    int         array_size; /* for TY_ARRAY, -1 = dynamic */
    int         is_const;
    /* For TY_FN_PTR */
    TypeRef    *ret_type;
    TypeRef   **param_types;
    int         param_count;
};

/* ─── Field (prop / state / computed variable) ────────────────────────────── */

struct Field {
    char    *name;
    TypeRef *type;
    char    *init_expr; /* raw C expression string, or NULL */
    int      is_reactive; /* set by analyzer: does template reference this? */
};

/* ─── Style Rule ──────────────────────────────────────────────────────────── */

struct StyleRule {
    char *property;   /* e.g. "background"  */
    char *value;      /* e.g. "props.color" — may be a C expression */
    int   is_dynamic; /* set by analyzer: references props/state?   */
};

/* ─── HTML Attribute ──────────────────────────────────────────────────────── */

struct Attribute {
    char *name;        /* e.g. "class", "onclick", "href" */
    char *value;       /* raw string or C expression       */
    int   is_expr;     /* 1 if value is a {} C expression  */
};

/* ─── HTML / Template Node ────────────────────────────────────────────────── */

typedef enum {
    HTML_ELEMENT,  /* <div class="x"> ... </div>  */
    HTML_TEXT,     /* plain text                   */
    HTML_EXPR,     /* {state.count}                */
    HTML_COMPONENT /* <Button label="x" />         */
} HtmlKind;

struct HtmlNode {
    HtmlKind    kind;
    char       *tag;         /* element tag name or component name  */
    Attribute  *attrs;       /* array of attributes                 */
    int         attr_count;
    HtmlNode   *children;    /* child nodes                         */
    int         child_count;
    char       *text;        /* for HTML_TEXT / HTML_EXPR           */
    int         self_closing;
};

/* ─── Event Handler ───────────────────────────────────────────────────────── */

typedef struct {
    char *event_name;  /* "click", "change", "submit", etc. */
    char *body;        /* raw C statement block             */
} EventHandler;

/* ─── Computed Field ──────────────────────────────────────────────────────── */

typedef struct {
    Field  field;       /* type + name                */
    char  *expression;  /* right-hand-side expression */
} ComputedField;

/* ─── Component Node (root of AST) ───────────────────────────────────────── */

typedef struct {
    char *name;        /* component name, e.g. "Button" */
    SrcLoc loc;

    /* Sections */
    Field         *props;
    int            prop_count;

    Field         *state;
    int            state_count;

    StyleRule     *style;
    int            style_count;

    EventHandler  *handlers;
    int            handler_count;

    ComputedField *computed;
    int            computed_count;

    HtmlNode      *template_root; /* root of the template HTML tree */

    /* Includes: list of #include paths */
    char         **includes;
    int            include_count;

    /* Reactivity graph (filled by analyzer) */
    int           *state_used_in_template;   /* bool array [state_count]  */
    int           *props_used_in_template;   /* bool array [prop_count]   */
} ComponentNode;

/* ─── Program (collection of components) ─────────────────────────────────── */

typedef struct {
    ComponentNode **components;
    int             component_count;
} Program;

/* ─── Allocator Helpers ───────────────────────────────────────────────────── */

ComponentNode *ast_new_component(void);
HtmlNode      *ast_new_html_node(HtmlKind kind);
TypeRef       *ast_new_type(TypeKind kind);
Field          ast_new_field(void);
void           ast_free_component(ComponentNode *c);
void           ast_free_program(Program *p);

/* ─── Debug Dump ──────────────────────────────────────────────────────────── */

void ast_dump_component(const ComponentNode *c, int indent);
void ast_dump_program(const Program *p);

#endif /* FORGE_AST_H */
