.DEFAULT_GOAL := help

.ONESHELL: # Applies to every targets

include env.mk

# --- Setup ---

# npm install is re-run only when package.json changes
node_modules/.package-lock.json: package.json package-lock.json
	npm install
	@touch $@

# PlatformIO is installed in a local Python virtualenv so the toolchain
# is reproducible and doesn't pollute the system Python.
.venv/bin/pio:
	python3 -m venv .venv
	.venv/bin/pip install --upgrade pip
	.venv/bin/pip install platformio

.PHONY: install-pio
install-pio: .venv/bin/pio ## Install PlatformIO in a local Python venv

.PHONY: prepare
prepare: node_modules/.package-lock.json ## Install git hooks
	husky

.PHONY: setup
setup: install install-pio prepare ## Full dev environment setup

.PHONY: install
install: node_modules/.package-lock.json ## Install dev tools (npm)

# --- Formatting ---

.PHONY: lint
lint: ## Check formatting with clang-format
	find lib/ -name '*.h' -o -name '*.cpp' -o -name '*.ino' | xargs clang-format --dry-run --Werror

.PHONY: lint-fix
lint-fix: ## Auto-fix formatting
	find lib/ -name '*.h' -o -name '*.cpp' -o -name '*.ino' | xargs clang-format -i

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
