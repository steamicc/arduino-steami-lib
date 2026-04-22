#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fail if any C/C++ source file is missing the SPDX-License-Identifier
# header on its very first line. Used by `make lint` (project-wide
# sweep) and by lint-staged (pre-commit, staged files only).
#
# Usage:
#   scripts/check-spdx.sh                 # scan lib/ src/ tests/
#   scripts/check-spdx.sh path/to/file    # check only the given files

set -eu

# Required literal form on line 1 of every C++ source. The SPDX spec is
# more permissive, but the project convention (see CONTRIBUTING.md) is
# this exact comment on the first line — matching strictly avoids
# accepting near-misses that drift further over time.
EXPECTED_LINE_PREFIX='// SPDX-License-Identifier:'

missing=0
missing_files=""

check_file() {
    file=$1

    # Restrict to the file types we actually lint, in case lint-staged
    # or a contributor passes something else through.
    case "$file" in
        *.h|*.cpp|*.ino) ;;
        *) return ;;
    esac

    [ -f "$file" ] || return

    first_line=$(head -n 1 "$file")
    case "$first_line" in
        "$EXPECTED_LINE_PREFIX"*) ;;
        *)
            missing=$((missing + 1))
            missing_files="${missing_files}${file}
"
            ;;
    esac
}

if [ $# -eq 0 ]; then
    # Project-wide sweep. Iterate line by line with IFS=newline so
    # filenames containing spaces survive unscathed. Newlines in
    # filenames aren't supported — they're absurd and the tree has
    # none.
    OLD_IFS=$IFS
    IFS='
'
    for file in $(find lib src tests -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.ino' \)); do
        check_file "$file"
    done
    IFS=$OLD_IFS
else
    for file in "$@"; do
        check_file "$file"
    done
fi

if [ "$missing" -gt 0 ]; then
    printf '%s' "$missing_files" >&2
    echo "" >&2
    echo "$missing file(s) missing the SPDX-License-Identifier header on line 1." >&2
    echo "Add this as the very first line of each file:" >&2
    echo "  // SPDX-License-Identifier: GPL-3.0-or-later" >&2
    exit 1
fi
