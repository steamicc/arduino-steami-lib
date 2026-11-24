#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compile every example of the Arduino Library Manager package with
# arduino-cli, as a user would after installing the STeaMi library.
#
# Requires arduino-cli on PATH with the STM32 core and STM32duinoBLE
# installed. The package is installed as <user dir>/libraries/STeaMi
# (replacing any previous copy).
#
# Usage:
#   scripts/compile-package-examples.sh PKG_DIR

set -euo pipefail

FQBN="STMicroelectronics:stm32:STeaMi"

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 PKG_DIR" >&2
    exit 2
fi

pkg_dir="$1"
if [ ! -f "$pkg_dir/library.properties" ]; then
    echo "error: $pkg_dir is not an Arduino package (no library.properties)" >&2
    exit 2
fi
pkg_dir="$(cd "$pkg_dir" && pwd)"

user_dir="$(arduino-cli config get directories.user)"
lib_dir="$user_dir/libraries/STeaMi"
mkdir -p "$user_dir/libraries"
rm -rf "$lib_dir"
cp -r "$pkg_dir" "$lib_dir"

total=0
passed=0
failed=0
log="$(mktemp)"
trap 'rm -f "$log"' EXIT

while IFS= read -r example; do
    [ -n "$example" ] || continue
    name="${example#"$pkg_dir"/examples/}"
    total=$((total + 1))
    if arduino-cli compile --fqbn "$FQBN" "$lib_dir/examples/$name" >"$log" 2>&1; then
        passed=$((passed + 1))
        echo "PASS $name"
    else
        failed=$((failed + 1))
        echo "FAIL $name"
        tail -n 15 "$log" | sed 's/^/    /'
    fi
done < <(find "$pkg_dir/examples" -mindepth 2 -maxdepth 2 -type d | sort)

echo "Total: $total | Passed: $passed | Failed: $failed"
[ "$failed" -eq 0 ]
