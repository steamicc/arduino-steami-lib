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
# With --output-dir, the compiled binary of each example is also copied to
# DIR/<COMPONENT>/<name>.bin (input of scripts/build-examples-archive.py).
#
# With --only FILE, compile just the examples listed in FILE, one
# lib/<driver>/examples/<example> per line (blank lines ignored), instead of
# every example. A malformed line, or an example the package does not hold,
# is an error (exit 2) and nothing is compiled.
#
# Usage:
#   scripts/compile-package-examples.sh PKG_DIR [--output-dir DIR] [--only FILE]

set -euo pipefail

FQBN="STMicroelectronics:stm32:STeaMi"

usage="Usage: $0 PKG_DIR [--output-dir DIR] [--only FILE]"
out_dir=""
only_file=""
if [ "$#" -lt 1 ]; then
    echo "$usage" >&2
    exit 2
fi
pkg_dir="$1"
shift
while [ "$#" -gt 0 ]; do
    case "$1" in
        --output-dir | --only)
            if [ "$#" -lt 2 ]; then
                echo "$usage" >&2
                exit 2
            fi
            if [ "$1" = "--output-dir" ]; then out_dir="$2"; else only_file="$2"; fi
            shift 2
            ;;
        *)
            echo "$usage" >&2
            exit 2
            ;;
    esac
done

if [ ! -f "$pkg_dir/library.properties" ]; then
    echo "error: $pkg_dir is not an Arduino package (no library.properties)" >&2
    exit 2
fi
pkg_dir="$(cd "$pkg_dir" && pwd)"
if [ -n "$out_dir" ]; then
    mkdir -p "$out_dir"
    out_dir="$(cd "$out_dir" && pwd)"
fi

# The examples to compile, as paths relative to $pkg_dir/examples.
names=()
if [ -n "$only_file" ]; then
    if [ ! -f "$only_file" ]; then
        echo "error: $only_file: no such file" >&2
        exit 2
    fi
    while IFS= read -r line || [ -n "$line" ]; do
        [ -n "$line" ] || continue
        if [[ ! "$line" =~ ^lib/([^/]+)/examples/([^/]+)$ ]]; then
            echo "error: malformed line '$line' (expected lib/<driver>/examples/<example>)" >&2
            exit 2
        fi
        driver="${BASH_REMATCH[1]}"
        name="${driver^^}/${BASH_REMATCH[2]}"
        if [ ! -d "$pkg_dir/examples/$name" ]; then
            echo "error: $line is not in the package (no examples/$name)" >&2
            exit 2
        fi
        names+=("$name")
    done <"$only_file"
else
    while IFS= read -r example; do
        names+=("${example#"$pkg_dir"/examples/}")
    done < <(find "$pkg_dir/examples" -mindepth 2 -maxdepth 2 -type d | sort)
fi

user_dir="$(arduino-cli config get directories.user)"
lib_dir="$user_dir/libraries/STeaMi"
mkdir -p "$user_dir/libraries"
rm -rf "$lib_dir"
cp -r "$pkg_dir" "$lib_dir"

total=0
passed=0
failed=0
log="$(mktemp)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$log" "$build_dir"' EXIT

# Compile one example (name = COMPONENT/example). With --output-dir, keep
# its binary as $out_dir/COMPONENT/example.bin.
compile_example() {
    local name="$1"
    local args=(--fqbn "$FQBN")
    if [ -z "$out_dir" ]; then
        arduino-cli compile "${args[@]}" "$lib_dir/examples/$name" >"$log" 2>&1
        return
    fi
    rm -rf "${build_dir:?}"/*
    rm -f "$out_dir/$name.bin" # never keep a binary from an earlier run
    arduino-cli compile "${args[@]}" --output-dir "$build_dir" \
        "$lib_dir/examples/$name" >"$log" 2>&1 || return 1
    mkdir -p "$out_dir/${name%/*}"
    cp "$build_dir/${name#*/}.ino.bin" "$out_dir/$name.bin" >>"$log" 2>&1
}

for name in ${names[@]+"${names[@]}"}; do
    total=$((total + 1))
    if compile_example "$name"; then
        passed=$((passed + 1))
        echo "PASS $name"
    else
        failed=$((failed + 1))
        echo "FAIL $name"
        tail -n 15 "$log" | sed 's/^/    /'
    fi
done

echo "Total: $total | Passed: $passed | Failed: $failed"
if [ -n "$out_dir" ] && [ "$total" -eq 0 ]; then
    echo "error: no example found in $pkg_dir/examples, nothing to archive" >&2
    exit 1
fi
[ "$failed" -eq 0 ]
