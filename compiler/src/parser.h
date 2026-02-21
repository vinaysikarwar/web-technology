/*
 * Forge Framework - Parser
 * Builds an AST from the token stream produced by the Lexer.
 */

#ifndef FORGE_PARSER_H
#define FORGE_PARSER_H

#include "lexer.h"
#include "ast.h"

/* ─── Parser State ────────────────────────────────────────────────────────── */

typedef struct {
    Lexer      *lex;
    Token       current;
    Token       previous;
    int         had_error;
    int         panic_mode;
} Parser;

/* ─── Public API ──────────────────────────────────────────────────────────── */

void     parser_init(Parser *p, Lexer *lex);
Program *parser_parse(Parser *p);          /* parse entire file → Program   */
void     parser_free(Parser *p);

/* Error count after parsing */
int parser_error_count(Parser *p);

#endif /* FORGE_PARSER_H */
