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

# Touch the target after pip install so make's mtime comparison stops
# re-firing — pip install of an already-satisfied package is a no-op
# that doesn't refresh the installed binary's timestamp, and without
# the touch the "requirements.txt newer than target" check stays true
# forever and pip runs on every make invocation.
.venv/bin/pio: requirements.txt | .venv
	.venv/bin/pip install -c requirements.txt platformio
	@test -x $@ && touch $@

.venv/bin/clang-format: requirements.txt | .venv
	.venv/bin/pip install -c requirements.txt clang-format
	@test -x $@ && touch $@

.venv/bin/clang-tidy: requirements.txt | .venv
	.venv/bin/pip install -c requirements.txt clang-tidy
	@test -x $@ && touch $@

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

# Driver sources analysed by clang-tidy. Listed via $(shell) so the set
# is current each time make runs.
DRIVER_SOURCES := $(shell find lib -type f -path '*/src/*.cpp')

# compile_commands.json is regenerated whenever platformio.ini, the board
# JSON, or the driver source list changes — clang-tidy needs the exact
# compile flags PIO used for every file it analyses. Adding a new driver
# forces a DB refresh so tidy doesn't fall back to guessed flags.
# Generated from the native env because host compile handles the STL /
# headers cleanly (the ARM cross-compile toolchain trips clang-tidy up).
compile_commands.json: .venv/bin/pio platformio.ini boards/steami.json $(DRIVER_SOURCES)
	pio run -t compiledb -e native

.PHONY: tidy
tidy: .venv/bin/clang-tidy compile_commands.json ## Run clang-tidy on every driver source under lib/*/src/
	@find lib -type f -path '*/src/*.cpp' -exec .venv/bin/clang-tidy -p . --quiet {} +

.PHONY: check-spdx
check-spdx: ## Verify every C++ source carries the SPDX license header
	@scripts/check-spdx.sh

.PHONY: lint
lint: format-check check-spdx tidy ## Run all static checks (format + SPDX + static analysis)

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
