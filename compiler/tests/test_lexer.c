/*
 * Forge Compiler — Lexer Tests
 * Run with: make test
 */

#include "../src/lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run    = 0;
static int tests_passed = 0;

#define ASSERT_EQ(a, b, msg) do {               \
    tests_run++;                                 \
    if ((a) == (b)) {                            \
        tests_passed++;                          \
        printf("  \033[32m✓\033[0m %s\n", msg); \
    } else {                                     \
        printf("  \033[31m✗\033[0m %s  (got %d, want %d)\n", msg, (int)(a), (int)(b)); \
    }                                            \
} while(0)

#define ASSERT_STR(a, b, msg) do {              \
    tests_run++;                                 \
    if (strcmp(a, b) == 0) {                     \
        tests_passed++;                          \
        printf("  \033[32m✓\033[0m %s\n", msg); \
    } else {                                     \
        printf("  \033[31m✗\033[0m %s  (got '%s', want '%s')\n", msg, a, b); \
    }                                            \
} while(0)

/* ─── Helpers ─────────────────────────────────────────────────────────────── */

static Token next_non_ws(Lexer *lex) {
    Token t;
    do { t = lexer_next(lex); } while (t.type == TOK_HTML_TEXT && t.length == 0);
    return t;
}

/* ─── Test Cases ──────────────────────────────────────────────────────────── */

static void test_basic_tokens(void) {
    printf("\ntest_basic_tokens\n");
    const char *src = "int count = 42;";
    Lexer lex;
    lexer_init(&lex, src, "test");

    Token t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_INT, "first token is 'int'");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_IDENT, "second token is identifier");
    char buf[32] = {0};
    memcpy(buf, t.start, t.length);
    ASSERT_STR(buf, "count", "identifier text is 'count'");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_ASSIGN, "token is '='");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_INT_LIT, "token is integer literal");
    ASSERT_EQ(t.value.int_val, 42, "integer value is 42");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_SEMICOLON, "token is ';'");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_EOF, "token is EOF");
}

static void test_forge_directives(void) {
    printf("\ntest_forge_directives\n");
    const char *src = "@component Button { @props { int x; } }";
    Lexer lex;
    lexer_init(&lex, src, "test");

    Token t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_AT_COMPONENT, "@component directive");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_IDENT, "component name identifier");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_LBRACE, "opening brace");

    t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_AT_PROPS, "@props directive");
}

static void test_string_literal(void) {
    printf("\ntest_string_literal\n");
    const char *src = "\"hello\\nworld\"";
    Lexer lex;
    lexer_init(&lex, src, "test");

    Token t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_STRING_LIT, "string literal token");
    ASSERT_STR(t.value.str_val, "hello\nworld", "escape sequence decoded");
    free(t.value.str_val);
}

static void test_operators(void) {
    printf("\ntest_operators\n");
    const char *src = "++ -- += -= == != <= >= && ||";
    Lexer lex;
    lexer_init(&lex, src, "test");

    ASSERT_EQ(lexer_next(&lex).type, TOK_PLUSPLUS,   "++");
    ASSERT_EQ(lexer_next(&lex).type, TOK_MINUSMINUS, "--");
    ASSERT_EQ(lexer_next(&lex).type, TOK_PLUSEQ,     "+=");
    ASSERT_EQ(lexer_next(&lex).type, TOK_MINUSEQ,    "-=");
    ASSERT_EQ(lexer_next(&lex).type, TOK_EQEQ,       "==");
    ASSERT_EQ(lexer_next(&lex).type, TOK_NEQ,        "!=");
    ASSERT_EQ(lexer_next(&lex).type, TOK_LT_EQ,      "<=");
    ASSERT_EQ(lexer_next(&lex).type, TOK_GT_EQ,      ">=");
    ASSERT_EQ(lexer_next(&lex).type, TOK_AND,        "&&");
    ASSERT_EQ(lexer_next(&lex).type, TOK_OR,         "||");
}

static void test_comments(void) {
    printf("\ntest_comments\n");
    const char *src = "int /* block */ x // line\n= 5;";
    Lexer lex;
    lexer_init(&lex, src, "test");

    ASSERT_EQ(lexer_next(&lex).type, TOK_INT,   "int after block comment");
    ASSERT_EQ(lexer_next(&lex).type, TOK_IDENT, "x skipping block comment");
    ASSERT_EQ(lexer_next(&lex).type, TOK_ASSIGN,"= skipping line comment");
    ASSERT_EQ(lexer_next(&lex).type, TOK_INT_LIT,"5");
}

static void test_line_numbers(void) {
    printf("\ntest_line_numbers\n");
    const char *src = "int\nfloat\nbool";
    Lexer lex;
    lexer_init(&lex, src, "test");

    Token t1 = lexer_next(&lex);
    ASSERT_EQ(t1.loc.line, 1, "int on line 1");

    Token t2 = lexer_next(&lex);
    ASSERT_EQ(t2.loc.line, 2, "float on line 2");

    Token t3 = lexer_next(&lex);
    ASSERT_EQ(t3.loc.line, 3, "bool on line 3");
}

static void test_hex_literal(void) {
    printf("\ntest_hex_literal\n");
    const char *src = "0xFF 0x1a2b";
    Lexer lex;
    lexer_init(&lex, src, "test");

    Token t = lexer_next(&lex);
    ASSERT_EQ(t.type, TOK_INT_LIT, "hex literal token");
    ASSERT_EQ(t.value.int_val, 0xFF, "hex value 0xFF = 255");

    t = lexer_next(&lex);
    ASSERT_EQ(t.value.int_val, 0x1a2b, "hex value 0x1a2b = 6699");
}

/* ─── Main ────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Forge Lexer Tests ===\n");

    test_basic_tokens();
    test_forge_directives();
    test_string_literal();
    test_operators();
    test_comments();
    test_line_numbers();
    test_hex_literal();

    printf("\n══════════════════════════\n");
    printf("  %d / %d tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("  \033[32mAll tests passed ✓\033[0m\n");
        return 0;
    } else {
        printf("  \033[31m%d tests FAILED\033[0m\n", tests_run - tests_passed);
        return 1;
    }
}
