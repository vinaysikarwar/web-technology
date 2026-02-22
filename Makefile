# ─── Forge Framework — Root Makefile ──────────────────────────────────────────
#
# Targets:
#   make             Build the compiler + runtime + dev server
#   make compiler    Build only the compiler (forge binary)
#   make runtime     Build only the runtime (forge_runtime.wasm)
#   make dev-server  Build only the dev server
#   make examples    Compile all examples
#   make test        Run tests
#   make clean       Remove all build artifacts
#   make install     Install forge to /usr/local/bin
# ──────────────────────────────────────────────────────────────────────────────

CC      := clang
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS :=

FORGE_VERSION := 0.1.0
PREFIX        := /usr/local

# ─── Directories ──────────────────────────────────────────────────────────────

BUILD_DIR        := build
COMPILER_SRC     := compiler/src
RUNTIME_SRC      := runtime/src
RUNTIME_INCLUDE  := runtime/include
DEV_SERVER_SRC   := tools/dev-server

# ─── Compiler Sources ─────────────────────────────────────────────────────────

COMPILER_SRCS := \
    $(COMPILER_SRC)/main.c       \
    $(COMPILER_SRC)/lexer.c      \
    $(COMPILER_SRC)/ast.c        \
    $(COMPILER_SRC)/parser.c     \
    $(COMPILER_SRC)/analyzer.c   \
    $(COMPILER_SRC)/codegen.c    \
    $(COMPILER_SRC)/wasm_emit.c  \
    $(COMPILER_SRC)/binding_gen.c

COMPILER_OBJS := $(patsubst $(COMPILER_SRC)/%.c, $(BUILD_DIR)/compiler/%.o, $(COMPILER_SRCS))

# ─── Runtime Sources ──────────────────────────────────────────────────────────

RUNTIME_SRCS := \
    $(RUNTIME_SRC)/arena.c       \
    $(RUNTIME_SRC)/registry.c    \
    $(RUNTIME_SRC)/forge_runtime.c

RUNTIME_OBJS := $(patsubst $(RUNTIME_SRC)/%.c, $(BUILD_DIR)/runtime/%.o, $(RUNTIME_SRCS))

# ─── Default Target ───────────────────────────────────────────────────────────

.PHONY: all compiler runtime dev-server examples test clean install help

all: compiler runtime dev-server
	@echo ""
	@echo "  \033[32m✓ Forge v$(FORGE_VERSION) built successfully\033[0m"
	@echo "  Compiler:   $(BUILD_DIR)/forge"
	@echo "  Runtime:    $(BUILD_DIR)/forge_runtime.a"
	@echo "  Dev Server: $(BUILD_DIR)/forge-dev"
	@echo ""

# ─── Compiler ─────────────────────────────────────────────────────────────────

compiler: $(BUILD_DIR)/forge

$(BUILD_DIR)/forge: $(COMPILER_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "  \033[32m✓\033[0m compiler → $@"

$(BUILD_DIR)/compiler/%.o: $(COMPILER_SRC)/%.c
	@mkdir -p $(BUILD_DIR)/compiler
	$(CC) $(CFLAGS) -I$(COMPILER_SRC) -c -o $@ $<

# ─── Runtime (Static Library for linking into WASM) ───────────────────────────

runtime: $(BUILD_DIR)/forge_runtime.a

$(BUILD_DIR)/forge_runtime.a: $(RUNTIME_OBJS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $^
	@echo "  \033[32m✓\033[0m runtime → $@"

$(BUILD_DIR)/runtime/%.o: $(RUNTIME_SRC)/%.c
	@mkdir -p $(BUILD_DIR)/runtime
	$(CC) $(CFLAGS) -I$(RUNTIME_INCLUDE) -c -o $@ $<

# ─── Dev Server ───────────────────────────────────────────────────────────────

dev-server: $(BUILD_DIR)/forge-dev

$(BUILD_DIR)/forge-dev: $(DEV_SERVER_SRC)/main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -lpthread -o $@ $<
	@echo "  \033[32m✓\033[0m dev-server → $@"

# ─── Compile Examples ─────────────────────────────────────────────────────────

examples: compiler
	@echo "Compiling examples..."
	$(BUILD_DIR)/forge compile --no-wasm --prerender -o examples/01-counter/dist \
	    examples/01-counter/Counter.cx examples/01-counter/App.cx
	$(BUILD_DIR)/forge compile --no-wasm --prerender -o examples/02-todo-app/dist \
	    examples/02-todo-app/TodoItem.cx examples/02-todo-app/App.cx
	$(BUILD_DIR)/forge compile --no-wasm --prerender -o examples/03-dashboard/dist \
	    examples/03-dashboard/Card.cx examples/03-dashboard/App.cx
	$(BUILD_DIR)/forge compile --no-wasm --prerender -o examples/04-property-site/dist \
	    examples/04-property-site/PropertyCard.cx examples/04-property-site/PropertyDetail.cx \
	    examples/04-property-site/ContactForm.cx \
	    examples/04-property-site/App.cx
	$(BUILD_DIR)/forge compile --no-wasm --prerender --ssr -o examples/05-ecommerce/dist \
	    examples/05-ecommerce/ProductCard.cx examples/05-ecommerce/ProductDetail.cx \
	    examples/05-ecommerce/CheckoutForm.cx \
	    examples/05-ecommerce/App.cx
	$(BUILD_DIR)/forge compile --no-wasm --prerender -o examples/06-medicine-shop/dist \
	    examples/06-medicine-shop/CategoryCard.cx examples/06-medicine-shop/MedicineCard.cx \
	    examples/06-medicine-shop/MedicineDetail.cx \
	    examples/06-medicine-shop/App.cx
	python3 inject_ssg.py
	python3 examples/05-ecommerce/inject.py
	cp examples/06-medicine-shop/base_index.html examples/06-medicine-shop/index.html
	@echo "  \033[32m✓\033[0m All examples compiled"

# ─── Tests ────────────────────────────────────────────────────────────────────

test: compiler
	@echo "Running tests..."
	$(CC) $(CFLAGS) -I$(COMPILER_SRC) \
	    $(COMPILER_SRC)/lexer.c \
	    $(COMPILER_SRC)/ast.c \
	    compiler/tests/test_lexer.c \
	    -o $(BUILD_DIR)/test_lexer
	$(BUILD_DIR)/test_lexer
	@echo "  \033[32m✓\033[0m All tests passed"

# ─── Install ──────────────────────────────────────────────────────────────────

install: all
	cp $(BUILD_DIR)/forge     $(PREFIX)/bin/forge
	cp $(BUILD_DIR)/forge-dev $(PREFIX)/bin/forge-dev
	@echo "  \033[32m✓\033[0m Installed to $(PREFIX)/bin/"

uninstall:
	rm -f $(PREFIX)/bin/forge $(PREFIX)/bin/forge-dev

# ─── Clean ────────────────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR)
	find examples -name 'dist' -type d -exec rm -rf {} + 2>/dev/null || true
	@echo "  \033[32m✓\033[0m Cleaned"

# ─── Help ─────────────────────────────────────────────────────────────────────

help:
	@echo ""
	@echo "  Forge Framework v$(FORGE_VERSION) — Build Targets"
	@echo ""
	@echo "  make            Build everything"
	@echo "  make compiler   Build the forge compiler"
	@echo "  make runtime    Build the forge_runtime.a library"
	@echo "  make dev-server Build the development server"
	@echo "  make examples   Compile all example components"
	@echo "  make test       Run test suite"
	@echo "  make install    Install to $(PREFIX)/bin"
	@echo "  make clean      Remove all build artifacts"
	@echo ""
