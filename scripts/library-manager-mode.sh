#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Print the arduino-lint "library-manager" mode to use for a library:
#   submit  when no library with exactly this name is in the Library
#           Manager index yet (first submission);
#   update  when the library is already indexed (later releases).
#
# The name is compared exactly but case-insensitively, as the Library
# Manager registry rule (LP017) does. Fails, printing nothing on stdout,
# if the index cannot be fetched or parsed: the mode is never guessed.
#
# Requires arduino-cli and python3 on PATH.
#
# Usage:
#   scripts/library-manager-mode.sh LIBRARY_NAME

set -euo pipefail

if [ "$#" -ne 1 ] || [ -z "$1" ]; then
    echo "Usage: $0 LIBRARY_NAME" >&2
    exit 2
fi

NAME="$1"

arduino-cli lib update-index >&2

# `lib search` matches loosely, so keep only exact name matches.
RESULT="$(arduino-cli lib search --names --format json "$NAME")"

printf '%s' "$RESULT" | python3 -I -c '
import json
import sys

name = sys.argv[1].casefold()
try:
    data = json.load(sys.stdin)
    libraries = data.get("libraries") or []
    names = [lib["name"] for lib in libraries]
except (ValueError, AttributeError, KeyError, TypeError) as err:
    sys.exit(f"Cannot parse the Library Manager index search: {err}")

print("update" if any(n.casefold() == name for n in names) else "submit")
' "$NAME"
