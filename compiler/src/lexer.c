/*
 * Forge Framework - Lexer Implementation
 *
 * Hand-written, zero-dependency tokenizer for .cx component files.
 * Handles mode-switching between C code blocks and HTML template blocks.
 */

#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── Keyword Table ───────────────────────────────────────────────────────── */

typedef struct { const char *word; TokenType type; } Keyword;

static const Keyword KEYWORDS[] = {
    /* Forge directives */
    { "component", TOK_AT_COMPONENT },
    { "props",     TOK_AT_PROPS     },
    { "state",     TOK_AT_STATE     },
    { "style",     TOK_AT_STYLE     },
    { "template",  TOK_AT_TEMPLATE  },
    { "on",        TOK_AT_ON        },
    { "computed",  TOK_AT_COMPUTED  },
    /* C types */
    { "int",       TOK_INT       },
    { "char",      TOK_CHAR      },
    { "bool",      TOK_BOOL      },
    { "float",     TOK_FLOAT     },
    { "double",    TOK_DOUBLE    },
    { "void",      TOK_VOID      },
    { "long",      TOK_LONG      },
    { "short",     TOK_SHORT     },
    { "unsigned",  TOK_UNSIGNED  },
    { "signed",    TOK_SIGNED    },
    { "struct",    TOK_STRUCT    },
    { "enum",      TOK_ENUM      },
    { "const",     TOK_CONST     },
    { "static",    TOK_STATIC    },
    { "extern",    TOK_EXTERN    },
    { "inline",    TOK_INLINE    },
    { "typedef",   TOK_TYPEDEF   },
    { "sizeof",    TOK_SIZEOF    },
    /* Control flow */
    { "if",        TOK_IF        },
    { "else",      TOK_ELSE      },
    { "for",       TOK_FOR       },
    { "while",     TOK_WHILE     },
    { "do",        TOK_DO        },
    { "return",    TOK_RETURN    },
    { "break",     TOK_BREAK     },
    { "continue",  TOK_CONTINUE  },
    { "switch",    TOK_SWITCH    },
    { "case",      TOK_CASE      },
    { "default",   TOK_DEFAULT   },
    /* Literals */
    { "true",      TOK_TRUE      },
    { "false",     TOK_FALSE     },
    { "NULL",      TOK_NULL      },
    { "null",      TOK_NULL      },
    { "include",   TOK_INCLUDE   },
    { NULL, 0 }
};

/* ─── Helpers ─────────────────────────────────────────────────────────────── */

static char peek_char(Lexer *lex) { return *lex->current; }
static char peek_next(Lexer *lex) {
    return lex->current[0] ? lex->current[1] : '\0';
}

static char advance(Lexer *lex) {
    char c = *lex->current++;
    if (c == '\n') {
        lex->line++;
        lex->line_start = lex->current;
    }
    return c;
}

static int match(Lexer *lex, char expected) {
    if (*lex->current == expected) { advance(lex); return 1; }
    return 0;
}

static void skip_whitespace(Lexer *lex) {
    for (;;) {
        char c = peek_char(lex);
        switch (c) {
        case ' ': case '\r': case '\t': case '\n':
            advance(lex); break;
        case '/':
            if (peek_next(lex) == '/') {   /* line comment */
                while (peek_char(lex) && peek_char(lex) != '\n') advance(lex);
            } else if (peek_next(lex) == '*') { /* block comment */
                advance(lex); advance(lex);
                while (peek_char(lex)) {
                    if (peek_char(lex) == '*' && peek_next(lex) == '/') {
                        advance(lex); advance(lex); break;
                    }
                    advance(lex);
                }
            } else { return; }
            break;
        default: return;
        }
    }
}

static Token make_token(Lexer *lex, TokenType type, const char *start) {
    Token tok;
    tok.type   = type;
    tok.start  = start;
    tok.length = (size_t)(lex->current - start);
    tok.loc.filename = lex->filename;
    tok.loc.line   = lex->line;
    tok.loc.column = (int)(start - lex->line_start) + 1;
    tok.value.str_val = NULL;
    return tok;
}

static Token error_token(Lexer *lex, const char *msg) {
    Token tok;
    tok.type   = TOK_ERROR;
    tok.start  = msg;
    tok.length = strlen(msg);
    tok.loc.filename = lex->filename;
    tok.loc.line   = lex->line;
    tok.loc.column = (int)(lex->current - lex->line_start) + 1;
    tok.value.str_val = NULL;
    return tok;
}

static TokenType ident_type(const char *start, size_t len, int after_at) {
    for (int i = 0; KEYWORDS[i].word; i++) {
        size_t klen = strlen(KEYWORDS[i].word);
        if (klen == len && memcmp(KEYWORDS[i].word, start, len) == 0) {
            if (after_at) {
                switch (KEYWORDS[i].type) {
                case TOK_AT_COMPONENT: case TOK_AT_PROPS:  case TOK_AT_STATE:
                case TOK_AT_STYLE:    case TOK_AT_TEMPLATE: case TOK_AT_ON:
                case TOK_AT_COMPUTED: return KEYWORDS[i].type;
                default: break;
                }
            } else {
                /* Forge directive keywords (state, props, etc.) should only
                 * be returned as TOK_AT_* when preceded by '@'.  Without '@',
                 * treat them as regular identifiers so that expressions like
                 * state.count or props.step work correctly in templates. */
                switch (KEYWORDS[i].type) {
                case TOK_AT_COMPONENT: case TOK_AT_PROPS:  case TOK_AT_STATE:
                case TOK_AT_STYLE:    case TOK_AT_TEMPLATE: case TOK_AT_ON:
                case TOK_AT_COMPUTED: return TOK_IDENT;
                default: return KEYWORDS[i].type;
                }
            }
        }
    }
    return TOK_IDENT;
}

/* ─── String Literal ──────────────────────────────────────────────────────── */

static Token lex_string(Lexer *lex) {
    const char *start = lex->current - 1; /* includes opening " */
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);

    while (peek_char(lex) && peek_char(lex) != '"') {
        char c = advance(lex);
        if (c == '\\') {
            char esc = advance(lex);
            switch (esc) {
            case 'n':  c = '\n'; break;
            case 't':  c = '\t'; break;
            case 'r':  c = '\r'; break;
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '0':  c = '\0'; break;
            default:   c = esc;  break;
            }
        }
        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = c;
    }
    if (!peek_char(lex)) {
        free(buf);
        return error_token(lex, "Unterminated string literal");
    }
    advance(lex); /* closing " */
    buf[len] = '\0';

    Token tok = make_token(lex, TOK_STRING_LIT, start);
    tok.value.str_val = buf;
    return tok;
}

/* ─── Number Literal ──────────────────────────────────────────────────────── */

static Token lex_number(Lexer *lex, const char *start) {
    int is_float = 0;

    /* Hex */
    if (*start == '0' && (peek_char(lex) == 'x' || peek_char(lex) == 'X')) {
        advance(lex);
        while (isxdigit(peek_char(lex))) advance(lex);
        Token tok = make_token(lex, TOK_INT_LIT, start);
        tok.value.int_val = strtoll(start, NULL, 16);
        return tok;
    }

    while (isdigit(peek_char(lex))) advance(lex);
    if (peek_char(lex) == '.' && isdigit(peek_next(lex))) {
        is_float = 1;
        advance(lex);
        while (isdigit(peek_char(lex))) advance(lex);
    }
    if (peek_char(lex) == 'e' || peek_char(lex) == 'E') {
        is_float = 1;
        advance(lex);
        if (peek_char(lex) == '+' || peek_char(lex) == '-') advance(lex);
        while (isdigit(peek_char(lex))) advance(lex);
    }
    /* Suffixes: f, u, l, ul, ll */
    while (peek_char(lex) == 'f' || peek_char(lex) == 'u' ||
           peek_char(lex) == 'l' || peek_char(lex) == 'L') advance(lex);

    Token tok = make_token(lex, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT, start);
    if (is_float) tok.value.float_val = atof(start);
    else          tok.value.int_val   = strtoll(start, NULL, 10);
    return tok;
}

/* ─── Template Mode Lexer ─────────────────────────────────────────────────── */

static Token lex_template_text(Lexer *lex) {
    const char *start = lex->current;
    while (peek_char(lex) &&
           peek_char(lex) != '<' &&
           peek_char(lex) != '{' &&
           peek_char(lex) != '}') {
        advance(lex);
    }
    return make_token(lex, TOK_HTML_TEXT, start);
}

static Token lex_template(Lexer *lex) {
    skip_whitespace(lex);
    const char *start = lex->current;
    char c = peek_char(lex);

    if (c == '\0') return make_token(lex, TOK_EOF, start);

    /* Expression block inside template */
    if (c == '{') {
        advance(lex);
        lex->expr_depth++;
        lex->mode = LEX_MODE_EXPR;
        return make_token(lex, TOK_LBRACE, start);
    }
    if (c == '}') {
        advance(lex);
        lex->template_depth--;
        if (lex->template_depth == 0) lex->mode = LEX_MODE_C;
        return make_token(lex, TOK_RBRACE, start);
    }
    if (c == '<') {
        advance(lex);
        return make_token(lex, TOK_LT, start);
    }
    if (c == '>') {
        advance(lex);
        return make_token(lex, TOK_GT, start);
    }
    if (c == '/') {
        advance(lex);
        return make_token(lex, TOK_SLASH, start);
    }
    if (c == '=') {
        advance(lex);
        return make_token(lex, TOK_ASSIGN, start);
    }

    /* Quoted attribute value "..." — read until closing quote */
    if (c == '"') {
        advance(lex); /* opening " */
        while (peek_char(lex) && peek_char(lex) != '"') {
            if (peek_char(lex) == '\\') advance(lex); /* escape */
            if (peek_char(lex)) advance(lex);
        }
        if (peek_char(lex) == '"') advance(lex); /* closing " */
        return make_token(lex, TOK_HTML_ATTR, start);
    }
    if (c == '\'') {
        advance(lex); /* opening ' */
        while (peek_char(lex) && peek_char(lex) != '\'') {
            if (peek_char(lex) == '\\') advance(lex);
            if (peek_char(lex)) advance(lex);
        }
        if (peek_char(lex) == '\'') advance(lex); /* closing ' */
        return make_token(lex, TOK_HTML_ATTR, start);
    }

    /* Tag names and attribute names */
    if (isalpha(c) || c == '_' || c == '-') {
        while (isalnum(peek_char(lex)) || peek_char(lex) == '_' || peek_char(lex) == '-')
            advance(lex);
        return make_token(lex, TOK_IDENT, start);
    }

    /* Plain text between tags (stops at structural delimiters) */
    return lex_template_text(lex);
}

/* ─── Style Mode Lexer ────────────────────────────────────────────────────── */

static Token lex_style(Lexer *lex) {
    skip_whitespace(lex);
    const char *start = lex->current;
    char c = peek_char(lex);

    if (c == '\0') return make_token(lex, TOK_EOF, start);
    if (c == '}') {
        /* This } closes the @style block itself */
        advance(lex);
        lex->mode = LEX_MODE_C;
        return make_token(lex, TOK_RBRACE, start);
    }
    if (c == '{') {
        /*
         * Dynamic value expression: {state.x ? "a" : "b"}
         * Consume the entire balanced {...} as a single HTML_ATTR token so
         * the inner } never triggers a mode switch or a premature RBRACE.
         */
        advance(lex); /* consume opening { */
        int depth = 1;
        while (peek_char(lex) && depth > 0) {
            char ch = peek_char(lex);
            if (ch == '{') depth++;
            else if (ch == '}') depth--;
            if (depth > 0) advance(lex); /* keep inner chars, stop before final } */
        }
        if (peek_char(lex) == '}') advance(lex); /* consume closing } */
        return make_token(lex, TOK_HTML_ATTR, start);
    }
    if (c == ':') { advance(lex); return make_token(lex, TOK_COLON, start); }
    if (c == ';') { advance(lex); return make_token(lex, TOK_SEMICOLON, start); }

    /* Static property name or value — read until a delimiter */
    while (peek_char(lex) && peek_char(lex) != ':' && peek_char(lex) != ';' &&
           peek_char(lex) != '{' && peek_char(lex) != '}') {
        advance(lex);
    }
    Token tok = make_token(lex, TOK_HTML_ATTR, start);
    /* trim trailing whitespace from the captured text */
    while (tok.length > 0 && isspace((unsigned char)tok.start[tok.length - 1])) tok.length--;
    return tok;
}

/* ─── Expression Mode (inside template {}) ────────────────────────────────── */

static Token lex_expr(Lexer *lex) {
    /* Expression mode: lex as C, but track { } to know when expression ends */
    skip_whitespace(lex);
    const char *start = lex->current;
    char c = peek_char(lex);
    if (c == '{') { advance(lex); lex->expr_depth++; return make_token(lex, TOK_LBRACE, start); }
    if (c == '}') {
        advance(lex);
        lex->expr_depth--;
        if (lex->expr_depth == 0) lex->mode = LEX_MODE_TEMPLATE;
        return make_token(lex, TOK_RBRACE, start);
    }
    /* fall through to C lexing below — handled in lexer_next */
    lex->mode = LEX_MODE_C;
    Token tok = lexer_next(lex);
    lex->mode = LEX_MODE_EXPR;
    return tok;
}

/* ─── Main Lexer ──────────────────────────────────────────────────────────── */

void lexer_init(Lexer *lex, const char *source, const char *filename) {
    lex->source         = source;
    lex->current        = source;
    lex->line_start     = source;
    lex->filename       = filename;
    lex->line           = 1;
    lex->mode           = LEX_MODE_C;
    lex->template_depth = 0;
    lex->expr_depth     = 0;
    lex->has_peek       = 0;
}

Token lexer_next(Lexer *lex) {
    if (lex->has_peek) { lex->has_peek = 0; return lex->peek; }

    if (lex->mode == LEX_MODE_TEMPLATE) return lex_template(lex);
    if (lex->mode == LEX_MODE_STYLE)    return lex_style(lex);
    if (lex->mode == LEX_MODE_EXPR)     return lex_expr(lex);

    skip_whitespace(lex);
    const char *start = lex->current;

    if (peek_char(lex) == '\0') return make_token(lex, TOK_EOF, start);

    char c = advance(lex);

    /* Preprocessor */
    if (c == '#') {
        while (isspace(peek_char(lex)) && peek_char(lex) != '\n') advance(lex);
        if (strncmp(lex->current, "include", 7) == 0) {
            lex->current += 7;
            return make_token(lex, TOK_INCLUDE, start);
        }
        return make_token(lex, TOK_HASH, start);
    }

    /* Forge directive — just return the token type.
     * Mode switching is handled explicitly by the parser AFTER it has consumed
     * the opening '{', so the lexer never reads that '{' in the wrong mode. */
    if (c == '@') {
        const char *ident_start = lex->current;
        while (isalnum(peek_char(lex)) || peek_char(lex) == '_') advance(lex);
        size_t len = (size_t)(lex->current - ident_start);
        TokenType t = ident_type(ident_start, len, 1);
        return make_token(lex, t, start);
    }

    /* Identifiers */
    if (isalpha(c) || c == '_') {
        while (isalnum(peek_char(lex)) || peek_char(lex) == '_') advance(lex);
        size_t len = (size_t)(lex->current - start);
        return make_token(lex, ident_type(start, len, 0), start);
    }

    /* Numbers */
    if (isdigit(c)) return lex_number(lex, start);

    /* Strings */
    if (c == '"') return lex_string(lex);

    /* Char literals */
    if (c == '\'') {
        char ch = advance(lex);
        if (ch == '\\') ch = advance(lex);
        if (peek_char(lex) == '\'') advance(lex);
        Token tok = make_token(lex, TOK_CHAR_LIT, start);
        tok.value.int_val = ch;
        return tok;
    }

    /* Operators and punctuation */
    switch (c) {
    case '{': return make_token(lex, TOK_LBRACE,    start);
    case '}': return make_token(lex, TOK_RBRACE,    start);
    case '(': return make_token(lex, TOK_LPAREN,    start);
    case ')': return make_token(lex, TOK_RPAREN,    start);
    case '[': return make_token(lex, TOK_LBRACKET,  start);
    case ']': return make_token(lex, TOK_RBRACKET,  start);
    case ';': return make_token(lex, TOK_SEMICOLON, start);
    case ',': return make_token(lex, TOK_COMMA,     start);
    case '.': return make_token(lex, TOK_DOT,       start);
    case ':': return make_token(lex, TOK_COLON,     start);
    case '?': return make_token(lex, TOK_QUESTION,  start);
    case '<':
        if (match(lex, '<'))  return make_token(lex, TOK_LSHIFT, start);
        if (match(lex, '='))  return make_token(lex, TOK_LT_EQ,  start);
        return make_token(lex, TOK_LT, start);
    case '>':
        if (match(lex, '>'))  return make_token(lex, TOK_RSHIFT, start);
        if (match(lex, '='))  return make_token(lex, TOK_GT_EQ,  start);
        return make_token(lex, TOK_GT, start);
    case '=':
        if (match(lex, '='))  return make_token(lex, TOK_EQEQ,   start);
        return make_token(lex, TOK_ASSIGN, start);
    case '!':
        if (match(lex, '='))  return make_token(lex, TOK_NEQ,    start);
        return make_token(lex, TOK_BANG, start);
    case '+':
        if (match(lex, '+'))  return make_token(lex, TOK_PLUSPLUS,  start);
        if (match(lex, '='))  return make_token(lex, TOK_PLUSEQ,    start);
        return make_token(lex, TOK_PLUS, start);
    case '-':
        if (match(lex, '-'))  return make_token(lex, TOK_MINUSMINUS, start);
        if (match(lex, '='))  return make_token(lex, TOK_MINUSEQ,    start);
        if (match(lex, '>'))  return make_token(lex, TOK_ARROW,      start);
        return make_token(lex, TOK_MINUS, start);
    case '*':
        if (match(lex, '='))  return make_token(lex, TOK_STAREQ,  start);
        return make_token(lex, TOK_STAR, start);
    case '/':
        if (match(lex, '='))  return make_token(lex, TOK_SLASHEQ, start);
        return make_token(lex, TOK_SLASH, start);
    case '%':
        if (match(lex, '='))  return make_token(lex, TOK_PERCENTEQ, start);
        return make_token(lex, TOK_PERCENT, start);
    case '&':
        if (match(lex, '&'))  return make_token(lex, TOK_AND,      start);
        if (match(lex, '='))  return make_token(lex, TOK_AMPEQ,    start);
        return make_token(lex, TOK_AMPERSAND, start);
    case '|':
        if (match(lex, '|'))  return make_token(lex, TOK_OR,       start);
        if (match(lex, '='))  return make_token(lex, TOK_PIPEEQ,   start);
        return make_token(lex, TOK_PIPE, start);
    case '^':
        if (match(lex, '='))  return make_token(lex, TOK_CARETEQ,  start);
        return make_token(lex, TOK_CARET, start);
    case '~': return make_token(lex, TOK_TILDE, start);
    default:  return error_token(lex, "Unexpected character");
    }
}

Token lexer_peek_token(Lexer *lex) {
    if (!lex->has_peek) {
        lex->peek     = lexer_next(lex);
        lex->has_peek = 1;
    }
    return lex->peek;
}

void lexer_set_mode(Lexer *lex, LexMode mode) { lex->mode = mode; }

/* ─── Utility ─────────────────────────────────────────────────────────────── */

const char *token_type_name(TokenType t) {
    switch (t) {
    case TOK_AT_COMPONENT: return "@component";
    case TOK_AT_PROPS:     return "@props";
    case TOK_AT_STATE:     return "@state";
    case TOK_AT_STYLE:     return "@style";
    case TOK_AT_TEMPLATE:  return "@template";
    case TOK_AT_ON:        return "@on";
    case TOK_AT_COMPUTED:  return "@computed";
    case TOK_INT:          return "int";
    case TOK_CHAR:         return "char";
    case TOK_BOOL:         return "bool";
    case TOK_FLOAT:        return "float";
    case TOK_IDENT:        return "identifier";
    case TOK_INT_LIT:      return "integer_literal";
    case TOK_FLOAT_LIT:    return "float_literal";
    case TOK_STRING_LIT:   return "string_literal";
    case TOK_LBRACE:       return "{";
    case TOK_RBRACE:       return "}";
    case TOK_LPAREN:       return "(";
    case TOK_RPAREN:       return ")";
    case TOK_SEMICOLON:    return ";";
    case TOK_COMMA:        return ",";
    case TOK_LT:           return "<";
    case TOK_GT:           return ">";
    case TOK_ASSIGN:       return "=";
    case TOK_EOF:          return "EOF";
    case TOK_ERROR:        return "ERROR";
    default:               return "?";
    }
}

void token_print(const Token *tok) {
    char buf[64] = {0};
    size_t n = tok->length < 63 ? tok->length : 63;
    strncpy(buf, tok->start, n);
    printf("[%s:%d:%d] %-16s '%s'\n",
           tok->loc.filename, tok->loc.line, tok->loc.column,
           token_type_name(tok->type), buf);
}

int token_is_type_keyword(TokenType t) {
    return t == TOK_INT    || t == TOK_CHAR  || t == TOK_BOOL   ||
           t == TOK_FLOAT  || t == TOK_DOUBLE|| t == TOK_VOID   ||
           t == TOK_LONG   || t == TOK_SHORT || t == TOK_UNSIGNED||
           t == TOK_SIGNED || t == TOK_STRUCT|| t == TOK_ENUM;
}
