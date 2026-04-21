.DEFAULT_GOAL := help

.ONESHELL: # Applies to every targets

include env.mk

# --- Setup ---

# npm install is re-run only when package.json changes
node_modules/.package-lock.json: package.json package-lock.json
	npm install
	@touch $@

# Python-based tooling (PlatformIO, clang-format, clang-tidy) is installed
# in a local virtualenv so nothing pollutes the system Python. Versions are
# pinned in requirements.txt; each tool target depends on that file so a
# version bump re-triggers pip install on the next make invocation.
.venv:
	python3 -m venv .venv
	.venv/bin/pip install --upgrade pip

.venv/bin/pio: requirements.txt | .venv
	.venv/bin/pip install -c requirements.txt platformio

.venv/bin/clang-format: requirements.txt | .venv
	.venv/bin/pip install -c requirements.txt clang-format

.venv/bin/clang-tidy: requirements.txt | .venv
	.venv/bin/pip install -c requirements.txt clang-tidy

.PHONY: install-pio
install-pio: .venv/bin/pio ## Install PlatformIO in the local venv

.PHONY: install-lint
install-lint: .venv/bin/clang-format .venv/bin/clang-tidy ## Install lint tools in the local venv

.PHONY: prepare
prepare: node_modules/.package-lock.json ## Install git hooks
	husky

.PHONY: setup
setup: install install-pio install-lint prepare ## Full dev environment setup

.PHONY: install
install: node_modules/.package-lock.json ## Install dev tools (npm)

# --- Formatting ---

.PHONY: format-check
format-check: .venv/bin/clang-format ## Check formatting with clang-format
	find lib/ src/ tests/ -name '*.h' -o -name '*.cpp' -o -name '*.ino' | xargs clang-format --dry-run --Werror

.PHONY: format-fix
format-fix: .venv/bin/clang-format ## Auto-fix formatting in place
	find lib/ src/ tests/ -name '*.h' -o -name '*.cpp' -o -name '*.ino' | xargs clang-format -i

# --- Linting ---
# lint is a meta-target — it aggregates every static check we run on the
# source tree. Each sub-target is runnable on its own.

.PHONY: tidy
tidy: .venv/bin/clang-tidy ## Run clang-tidy static analysis (scaffold — see #107)
	@echo "clang-tidy scaffold: full integration tracked in issue #107"
	@echo "  (needs compile_commands.json via 'pio run -t compiledb' + rule preset)"

.PHONY: lint
lint: format-check tidy ## Run all static checks (format + static analysis)

# --- Build ---

.PHONY: build
build: .venv/bin/pio ## Build with PlatformIO
	pio run

.PHONY: upload
upload: .venv/bin/pio ## Upload to board
	pio run --target upload --upload-port $(PORT)

# --- Testing ---

.PHONY: test-native
test-native: .venv/bin/pio ## Run host-side native tests (no board required)
	pio test -e native

.PHONY: test-hardware
test-hardware: .venv/bin/pio ## Run on-board hardware tests (STeaMi required)
	pio test -e steami

# --- CI ---

.PHONY: ci
ci: lint build test-native ## Run all CI checks (lint + build + native tests, no board required)

# --- Utilities ---

.PHONY: clean
clean: ## Remove build artifacts
	rm -rf .pio

.PHONY: deepclean
deepclean: clean ## Remove everything including node_modules and venv
	@if [ -d node_modules ]; then rm -rf node_modules && echo "node_modules removed"; else echo "node_modules not found, skipping"; fi
	@if [ -d .venv ]; then rm -rf .venv && echo ".venv removed"; else echo ".venv not found, skipping"; fi

.PHONY: help
help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' Makefile | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2}'

# A useful debug Make Target
.PHONY: printvars
printvars:
	@$(foreach V,$(sort $(.VARIABLES)), \
	$(if $(filter-out environment% default automatic, \
	$(origin $V)),$(warning $V=$($V) ($(value $V)))))

# List all available targets
.PHONY: list
list:
	@LC_ALL=C $(MAKE) -pRrq -f $(firstword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/(^|\n)# Files(\n|$$)/,/(^|\n)# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | grep -E -v -e '^[^[:alnum:]]' -e '^$$@$$'
