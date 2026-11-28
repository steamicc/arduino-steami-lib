#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""List the examples a change can break, so that CI compiles only those.

Usage:
    python3 scripts/exemples-concernes.py --base <sha> --head <sha> [--racine <dir>]

The changed files come from `git diff --name-only <base>...<head>`. The script
prints one line of compact JSON on the standard output:

    {"compiler": bool, "tout": bool, "exemples": [...], "pio_exemples": [...],
     "pilotes": [...]}

- `exemples` and `pio_exemples`: sorted `lib/<driver>/examples/<example>` folders
  (all the examples to compile, and one example per driver for PlatformIO);
- `pilotes`: sorted driver folder names concerned by the change;
- `tout`: a shared or unknown file changed, everything is compiled;
- `compiler`: something has to be compiled (or checked) at all.

The selection works per example: an example is concerned when the drivers it
includes (transitively) contain a driver touched by the change. Exit code 2 if
git fails.

The working tree at --racine must be checked out at --head (or at the PR merge
commit): the existence of `<example>.ino` and the `#include` lines are read from
the working tree, not from git. Renames are listed as a deletion plus an
addition (`--no-renames`), so a path moved out of a compile-relevant place still
counts.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
import subprocess
import sys
from pathlib import Path

DEFAULT_ROOT = Path(__file__).resolve().parent.parent

# Line-anchored: a string literal or a comment that mentions #include does not count.
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
UMBRELLA_HEADER = "STeaMi.h"
SOURCE_SUFFIXES = (".h", ".hpp", ".cpp", ".c", ".ino")

EXAMPLE_RE = re.compile(r"^lib/([^/]+)/examples/([^/]+)/.+$")
SRC_RE = re.compile(r"^lib/([^/]+)/(?:src/.+|library\.properties)$")

PACKAGE_PATTERNS = (
    "lib/*/keywords.txt",
    "scripts/arduino-package/*",
    "scripts/library-manager-mode.sh",
)

# Files without any effect on a compilation. tests/shared/** is not in this list:
# platformio.ini adds it with -I to the steami environment.
NO_EFFECT_PATTERNS = (
    "docs/*",
    "LICENSE",
    "tests/native/*",
    "tests/hardware/*",
    "tests/integration/*",
    "tests/qualification/*",
    "scripts/tests/*",
    ".github/ISSUE_TEMPLATE/*",
    ".github/workflows/auto-assign-author.yml",
    ".github/workflows/auto-label-pr.yml",
    ".github/workflows/check-commits.yml",
    ".github/workflows/codeql.yml",
    ".github/workflows/milestone-from-issue.yml",
    ".github/workflows/project-sync.yml",
    ".github/workflows/release.yml",
    ".github/workflows/publish-arduino-package.yml",
    ".github/workflows/examples-archive.yml",
    ".husky/*",
    ".vscode/*",
    ".clang-format",
    ".clang-tidy",
    ".editorconfig",
    ".gitattributes",
    ".gitignore",
    ".prettierrc.json",
    ".releaserc.json",
    ".validate-branch-namerc.js",
    "commitlint.config.js",
    "git-precommit-checks.config.js",
    "package.json",
    "package-lock.json",
    "ruff.toml",
    "scripts/build-examples-archive.py",
    "scripts/capture-serial.py",
    "scripts/gen-spleen-fonts.py",
    "scripts/mockup-review.py",
    "scripts/project-backfill.py",
    "scripts/qualify.py",
    "scripts/check-spdx.sh",
)


def _matches(chemin: str, motifs) -> bool:
    return any(fnmatch.fnmatchcase(chemin, motif) for motif in motifs)


def sans_effet(chemin: str) -> bool:
    if chemin.endswith(".md") and not fnmatch.fnmatchcase(chemin, "lib/*/examples/*"):
        return True
    return _matches(chemin, NO_EFFECT_PATTERNS)


def inclusions(texte: str) -> list[str]:
    """Header names of the `#include` lines of a source text."""
    return INCLUDE_RE.findall(texte)


def _lire(chemin: Path) -> str:
    return chemin.read_text(encoding="utf-8", errors="replace")


def pilotes_du_depot(racine: Path) -> list[str]:
    """Driver folders `lib/<p>` that have a src/ or examples."""
    return sorted(
        d.name
        for d in (racine / "lib").iterdir()
        if (d / "src").is_dir() or (d / "examples").is_dir()
    )


def pilotes_avec_src(racine: Path) -> list[str]:
    return sorted(d.name for d in (racine / "lib").iterdir() if (d / "src").is_dir())


def table_en_tetes(racine: Path) -> dict[str, str]:
    """Header file name -> owner driver, for every lib/<p>/src/*.h."""
    table: dict[str, str] = {}
    for entete in sorted(racine.glob("lib/*/src/*.h")):
        table.setdefault(entete.name, entete.parent.parent.name)
    return table


def proprietaires(noms: list[str], table: dict[str, str], racine: Path) -> set[str]:
    """Drivers owning the headers `noms`. The umbrella header or an unknown
    Steami*.h header means every driver."""
    resultat: set[str] = set()
    for nom in noms:
        base = nom.rsplit("/", 1)[-1]
        if base in table:
            resultat.add(table[base])
        elif base == UMBRELLA_HEADER or re.match(r"(?i)steami.*\.h$", base):
            resultat.update(pilotes_avec_src(racine))
    return resultat


def dependances_directes(racine: Path) -> dict[str, set[str]]:
    """Driver -> drivers included by its lib/<p>/src files (itself excluded)."""
    table = table_en_tetes(racine)
    resultat: dict[str, set[str]] = {}
    for pilote in pilotes_avec_src(racine):
        noms: list[str] = []
        for fichier in sorted((racine / "lib" / pilote / "src").rglob("*")):
            if fichier.is_file() and fichier.suffix in SOURCE_SUFFIXES:
                noms += inclusions(_lire(fichier))
        resultat[pilote] = proprietaires(noms, table, racine) - {pilote}
    return resultat


def fermeture(depart: set[str], directes: dict[str, set[str]]) -> set[str]:
    vus: set[str] = set()
    pile = list(depart)
    while pile:
        p = pile.pop()
        if p in vus:
            continue
        vus.add(p)
        pile.extend(directes.get(p, ()))
    return vus


def exemples_du_depot(racine: Path) -> list[str]:
    """Every `lib/<p>/examples/<e>` folder that holds `<e>.ino`, sorted."""
    return sorted(
        str(ino.parent.relative_to(racine))
        for ino in racine.glob("lib/*/examples/*/*.ino")
        if ino.stem == ino.parent.name
    )


def _par_pilote(exemples: list[str]) -> dict[str, list[str]]:
    groupes: dict[str, list[str]] = {}
    for exemple in sorted(exemples):
        groupes.setdefault(exemple.split("/")[1], []).append(exemple)
    return groupes


def _premiers(dossiers, exemples_complets: list[str]) -> list[str]:
    groupes = _par_pilote(exemples_complets)
    return sorted(groupes[d][0] for d in dossiers if d in groupes)


def _inclusions_du_croquis(racine: Path, exemple: str) -> list[str]:
    noms: list[str] = []
    for fichier in sorted((racine / exemple).rglob("*")):
        if fichier.is_file() and fichier.suffix in SOURCE_SUFFIXES:
            noms += inclusions(_lire(fichier))
    return noms


def selectionner(fichiers: list[str], racine: Path) -> dict:
    racine = Path(racine)
    touches: set[str] = set()
    exemples_modifies: set[str] = set()
    paquet = False
    tout = False

    for chemin in fichiers:
        m = EXAMPLE_RE.match(chemin)
        if m:
            exemple = f"lib/{m.group(1)}/examples/{m.group(2)}"
            if (racine / exemple / f"{m.group(2)}.ino").is_file():
                exemples_modifies.add(exemple)
            continue
        if re.match(r"^lib/[^/]+/examples/\.gitkeep$", chemin):
            continue
        m = SRC_RE.match(chemin)
        if m:
            touches.add(m.group(1))
            continue
        if _matches(chemin, PACKAGE_PATTERNS):
            paquet = True
            continue
        if sans_effet(chemin):
            continue
        tout = True

    complets = exemples_du_depot(racine)
    groupes = _par_pilote(complets)

    if tout:
        return {
            "compiler": True,
            "tout": True,
            "exemples": complets,
            "pio_exemples": _premiers(groupes, complets),
            "pilotes": pilotes_du_depot(racine),
        }

    directes = dependances_directes(racine)
    table = table_en_tetes(racine)
    avec_src = set(directes)

    exemples = set(exemples_modifies)
    if touches:
        for exemple in complets:
            pilote = exemple.split("/")[1]
            depart = proprietaires(_inclusions_du_croquis(racine, exemple), table, racine)
            if pilote in avec_src:
                depart.add(pilote)
            if fermeture(depart, directes) & touches:
                exemples.add(exemple)
        if not exemples and complets:
            # The flattened package compiles every .cpp in each sketch.
            exemples.add(complets[0])

    concernes = set(touches) | {e.split("/")[1] for e in exemples_modifies}
    for pilote, deps in directes.items():
        if fermeture(deps, directes) & touches:
            concernes.add(pilote)

    return {
        "compiler": bool(touches or exemples_modifies or paquet),
        "tout": False,
        "exemples": sorted(exemples),
        "pio_exemples": _premiers(concernes, complets),
        "pilotes": sorted(concernes),
    }


def fichiers_modifies(base: str, head: str, racine: Path) -> list[str]:
    proc = subprocess.run(
        ["git", "-C", str(racine), "diff", "--name-only", "--no-renames", "-z", f"{base}...{head}"],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or "git diff failed")
    return [f for f in proc.stdout.split("\0") if f]


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--base", required=True, help="base commit of the change")
    parser.add_argument("--head", required=True, help="head commit of the change")
    parser.add_argument("--racine", type=Path, default=DEFAULT_ROOT, help="repository root")
    args = parser.parse_args(argv)
    try:
        fichiers = fichiers_modifies(args.base, args.head, args.racine)
    except (RuntimeError, OSError) as erreur:
        print(f"git failed: {erreur}", file=sys.stderr)
        return 2
    print(json.dumps(selectionner(fichiers, args.racine), separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
