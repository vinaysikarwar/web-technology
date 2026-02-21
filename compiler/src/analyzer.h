/*
 * Forge Framework - Semantic Analyzer
 *
 * Validates the AST and builds the reactivity dependency graph:
 *   - Which state fields are referenced in the template?
 *   - Which props fields are referenced?
 *   - Which style rules are dynamic?
 *   - Type-checks attribute assignments.
 */

#ifndef FORGE_ANALYZER_H
#define FORGE_ANALYZER_H

#include "ast.h"

/* ─── Analysis Result ─────────────────────────────────────────────────────── */

typedef struct {
    int error_count;
    int warning_count;
} AnalysisResult;

/* ─── Public API ──────────────────────────────────────────────────────────── */

AnalysisResult analyze_program(Program *p);
AnalysisResult analyze_component(ComponentNode *c);

#endif /* FORGE_ANALYZER_H */
