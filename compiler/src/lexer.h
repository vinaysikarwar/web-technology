/*
 * Forge Framework - Lexer
 * Tokenizes .cx (C-eXtension) component files
 *
 * The lexer operates in two modes:
 *   LEX_MODE_C       - Standard C code sections
 *   LEX_MODE_TEMPLATE - HTML-like template sections
 */

#ifndef FORGE_LEXER_H
#define FORGE_LEXER_H

#include <stddef.h>
#include <stdint.h>

/* ─── Token Types ─────────────────────────────────────────────────────────── */

typedef enum {
    /* Forge directives */
    TOK_AT_COMPONENT,   /* @component */
    TOK_AT_PROPS,       /* @props     */
    TOK_AT_STATE,       /* @state     */
    TOK_AT_STYLE,       /* @style     */
    TOK_AT_TEMPLATE,    /* @template  */
    TOK_AT_ON,          /* @on        */
    TOK_AT_COMPUTED,    /* @computed  */

    /* C primitive types */
    TOK_INT,    TOK_CHAR,   TOK_BOOL,   TOK_FLOAT,
    TOK_DOUBLE, TOK_VOID,   TOK_LONG,   TOK_SHORT,
    TOK_UNSIGNED, TOK_SIGNED, TOK_STRUCT, TOK_ENUM,
    TOK_CONST,  TOK_STATIC, TOK_EXTERN, TOK_INLINE,
    TOK_TYPEDEF, TOK_SIZEOF,

    /* Control flow */
    TOK_IF, TOK_ELSE, TOK_FOR, TOK_WHILE, TOK_DO,
    TOK_RETURN, TOK_BREAK, TOK_CONTINUE, TOK_SWITCH,
    TOK_CASE, TOK_DEFAULT,

    /* Boolean / null literals */
    TOK_TRUE, TOK_FALSE, TOK_NULL,

    /* Preprocessor */
    TOK_HASH,       /* #  */
    TOK_INCLUDE,    /* include keyword after # */

    /* Identifiers & literals */
    TOK_IDENT,
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,
    TOK_CHAR_LIT,

    /* Punctuation */
    TOK_LBRACE,     /* { */
    TOK_RBRACE,     /* } */
    TOK_LPAREN,     /* ( */
    TOK_RPAREN,     /* ) */
    TOK_LBRACKET,   /* [ */
    TOK_RBRACKET,   /* ] */
    TOK_SEMICOLON,  /* ; */
    TOK_COMMA,      /* , */
    TOK_DOT,        /* . */
    TOK_COLON,      /* : */
    TOK_QUESTION,   /* ? */

    /* HTML tokens (active in template mode) */
    TOK_LT,         /* < */
    TOK_GT,         /* > */
    TOK_SLASH,      /* / */
    TOK_HTML_TEXT,  /* raw text between tags */
    TOK_HTML_ATTR,  /* attribute name */

    /* Arithmetic operators */
    TOK_PLUS,       /* + */
    TOK_MINUS,      /* - */
    TOK_STAR,       /* * */
    TOK_PERCENT,    /* % */
    TOK_AMPERSAND,  /* & */
    TOK_PIPE,       /* | */
    TOK_CARET,      /* ^ */
    TOK_BANG,       /* ! */
    TOK_TILDE,      /* ~ */
    TOK_LSHIFT,     /* << */
    TOK_RSHIFT,     /* >> */

    /* Increment / decrement */
    TOK_PLUSPLUS,   /* ++ */
    TOK_MINUSMINUS, /* -- */
    TOK_ARROW,      /* -> */

    /* Assignment operators */
    TOK_ASSIGN,     /* = */
    TOK_PLUSEQ,     /* += */
    TOK_MINUSEQ,    /* -= */
    TOK_STAREQ,     /* *= */
    TOK_SLASHEQ,    /* /= */
    TOK_PERCENTEQ,  /* %= */
    TOK_AMPEQ,      /* &= */
    TOK_PIPEEQ,     /* |= */
    TOK_CARETEQ,    /* ^= */

    /* Comparison operators */
    TOK_EQEQ,       /* == */
    TOK_NEQ,        /* != */
    TOK_LT_EQ,      /* <= */
    TOK_GT_EQ,      /* >= */

    /* Logical operators */
    TOK_AND,        /* && */
    TOK_OR,         /* || */

    /* Special */
    TOK_EOF,
    TOK_ERROR
} TokenType;

/* ─── Lexer Mode ──────────────────────────────────────────────────────────── */

typedef enum {
    LEX_MODE_C,         /* Parsing C code sections      */
    LEX_MODE_TEMPLATE,  /* Parsing @template HTML body   */
    LEX_MODE_EXPR,      /* Inside {} within a template   */
    LEX_MODE_STYLE      /* Parsing @style property:value */
} LexMode;

/* ─── Source Location ─────────────────────────────────────────────────────── */

typedef struct {
    const char *filename;
    int         line;
    int         column;
} SrcLoc;

/* ─── Token ───────────────────────────────────────────────────────────────── */

typedef struct {
    TokenType   type;
    const char *start;   /* pointer into source buffer */
    size_t      length;
    SrcLoc      loc;

    union {
        int64_t  int_val;
        double   float_val;
        char    *str_val;   /* heap-allocated, null-terminated */
    } value;
} Token;

/* ─── Lexer State ─────────────────────────────────────────────────────────── */

typedef struct {
    const char *source;     /* full source text */
    const char *current;    /* scanning position */
    const char *line_start; /* start of current line */
    const char *filename;
    int         line;
    LexMode     mode;
    int         template_depth; /* nesting depth inside @template */
    int         expr_depth;     /* { } depth inside expressions   */
    Token       peek;           /* one-token lookahead             */
    int         has_peek;
} Lexer;

/* ─── Public API ──────────────────────────────────────────────────────────── */

void  lexer_init(Lexer *lex, const char *source, const char *filename);
Token lexer_next(Lexer *lex);
Token lexer_peek_token(Lexer *lex);
void  lexer_set_mode(Lexer *lex, LexMode mode);

/* Utility */
const char *token_type_name(TokenType t);
void        token_print(const Token *tok);
int         token_is_type_keyword(TokenType t);

#endif /* FORGE_LEXER_H */
