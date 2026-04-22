#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fail if any C/C++ source file is missing the SPDX-License-Identifier
# header. Used by `make lint` (project-wide sweep) and by lint-staged
# (pre-commit, staged files only).
#
# Usage:
#   scripts/check-spdx.sh                 # scan lib/ src/ tests/
#   scripts/check-spdx.sh path/to/file    # check only the given files

set -eu

# Only inspect the first few lines so a stray "SPDX-License-Identifier"
# somewhere in the body doesn't count as a valid header.
SCAN_LINES=5

missing=0
missing_files=""

if [ $# -eq 0 ]; then
    # Default project-wide sweep. Use -print0 + xargs would be safer for
    # weird filenames, but this repo's tree is known-clean.
    set -- $(find lib src tests -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.ino' \))
fi

for file in "$@"; do
    # Restrict to the file types we actually lint, in case lint-staged
    # or a contributor passes something else through.
    case "$file" in
        *.h|*.cpp|*.ino) ;;
        *) continue ;;
    esac

    if [ ! -f "$file" ]; then
        continue
    fi

    if ! head -n "$SCAN_LINES" "$file" | grep -q 'SPDX-License-Identifier'; then
        missing=$((missing + 1))
        missing_files="$missing_files$file\n"
    fi
done

if [ "$missing" -gt 0 ]; then
    printf '%b' "$missing_files" >&2
    echo "" >&2
    echo "$missing file(s) missing the SPDX-License-Identifier header." >&2
    echo "Add this as the very first line of each file:" >&2
    echo "  // SPDX-License-Identifier: GPL-3.0-or-later" >&2
    exit 1
fi
