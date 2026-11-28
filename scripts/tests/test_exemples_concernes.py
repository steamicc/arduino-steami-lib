# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for scripts/exemples-concernes.py."""

from __future__ import annotations

import importlib.util
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "exemples-concernes.py"

TOUS = [
    "lib/a/examples/A1",
    "lib/a/examples/A2",
    "lib/b/examples/B1",
    "lib/ble/examples/E1",
    "lib/c/examples/C1",
]
PIO_TOUS = ["lib/a/examples/A1", "lib/b/examples/B1", "lib/ble/examples/E1", "lib/c/examples/C1"]
PILOTES_TOUS = ["a", "b", "ble", "c", "d"]
RIEN = {"compiler": False, "tout": False, "exemples": [], "pio_exemples": [], "pilotes": []}

# Fake tree: path -> content.
ARBRE = {
    "lib/a/library.properties": "name=STeaMi A\n",
    "lib/a/src/SteamiA.h": "",
    "lib/a/src/SteamiA.cpp": '#include "SteamiA.h"\n',
    "lib/a/examples/A1/A1.ino": "#include <SteamiA.h>\n",
    "lib/a/examples/A2/A2.ino": "#include <SteamiA.h>\n",
    "lib/b/library.properties": "name=STeaMi B\ndepends=STeaMi A\n",
    "lib/b/src/SteamiB.h": "#include <SteamiA.h>\n",
    "lib/b/examples/B1/B1.ino": "#include <SteamiB.h>\n",
    "lib/c/library.properties": "name=STeaMi C\n",
    "lib/c/src/SteamiC.h": "",
    "lib/c/examples/C1/C1.ino": "#include <SteamiC.h>\n#include <SteamiB.h>\n",
    "lib/d/library.properties": "name=STeaMi D\n",
    "lib/d/src/SteamiD.h": "",
    "lib/d/src/SteamiD.cpp": "",
    "lib/ble/examples/E1/E1.ino": "#include <SteamiC.h>\n",
}


def load_module():
    spec = importlib.util.spec_from_file_location("exemples_concernes", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write(racine: Path, chemin: str, contenu: str) -> None:
    cible = racine / chemin
    cible.parent.mkdir(parents=True, exist_ok=True)
    cible.write_text(contenu, encoding="utf-8")


class ExemplesConcernesTest(unittest.TestCase):
    def setUp(self) -> None:
        self.mod = load_module()
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.racine = Path(self._tmp.name)
        for chemin, contenu in ARBRE.items():
            write(self.racine, chemin, contenu)

    def sel(self, fichiers):
        return self.mod.selectionner(fichiers, self.racine)

    def test_doc_seule(self):
        self.assertEqual(
            self.sel(
                [
                    "README.md",
                    "docs/x.md",
                    "lib/a/README.md",
                    "tests/native/test_a/test_main.cpp",
                    "scripts/tests/test_x.py",
                ]
            ),
            RIEN,
        )

    def test_exemple_seul(self):
        self.assertEqual(
            self.sel(["lib/c/examples/C1/C1.ino"]),
            {
                "compiler": True,
                "tout": False,
                "exemples": ["lib/c/examples/C1"],
                "pio_exemples": ["lib/c/examples/C1"],
                "pilotes": ["c"],
            },
        )

    def test_gitkeep_sans_effet(self):
        self.assertEqual(self.sel(["lib/a/examples/.gitkeep"]), RIEN)

    def test_pilote_et_dependants(self):
        r = self.sel(["lib/a/src/SteamiA.cpp"])
        self.assertEqual(
            r["exemples"],
            [
                "lib/a/examples/A1",
                "lib/a/examples/A2",
                "lib/b/examples/B1",
                "lib/c/examples/C1",
            ],
        )
        self.assertEqual(r["pio_exemples"], ["lib/a/examples/A1", "lib/b/examples/B1"])
        self.assertEqual(r["pilotes"], ["a", "b"])  # c n'inclut a que par son exemple

    def test_pilote_sans_exemple(self):
        r = self.sel(["lib/d/src/SteamiD.cpp"])
        self.assertEqual((r["exemples"], r["pio_exemples"]), (["lib/a/examples/A1"], []))
        self.assertTrue(r["compiler"])
        self.assertEqual(r["pilotes"], ["d"])

    def test_metadonnees_du_pilote(self):
        r = self.sel(["lib/c/library.properties"])
        self.assertEqual(r["exemples"], ["lib/ble/examples/E1", "lib/c/examples/C1"])
        self.assertEqual(r["pio_exemples"], ["lib/c/examples/C1"])

    def test_paquet_seul(self):
        self.assertEqual(
            self.sel(["lib/a/keywords.txt"]),
            {"compiler": True, "tout": False, "exemples": [], "pio_exemples": [], "pilotes": []},
        )

    def test_fichier_partage_ou_inconnu(self):
        partages = (
            "platformio.ini",
            "tests/shared/x.h",
            ".github/workflows/ci.yml",
            "outil/nouveau.txt",
        )
        for f in partages:
            with self.subTest(f):
                self.assertEqual(
                    self.sel([f]),
                    {
                        "compiler": True,
                        "tout": True,
                        "exemples": TOUS,
                        "pio_exemples": PIO_TOUS,
                        "pilotes": PILOTES_TOUS,
                    },
                )

    def test_exemple_supprime(self):
        # A3.ino n'existe pas dans l'arbre de head : l'exemple est supprime.
        # Il n'ajoute rien (ni exemple, ni pilote) et ne demande pas de compilation.
        self.assertEqual(self.sel(["lib/a/examples/A3/A3.ino"]), RIEN)
        # Les autres chemins gardent leur effet.
        r = self.sel(["lib/a/examples/A3/A3.ino", "lib/c/examples/C1/C1.ino"])
        self.assertEqual(r["exemples"], ["lib/c/examples/C1"])
        self.assertEqual(r["pilotes"], ["c"])
        self.assertTrue(r["compiler"])

    def test_dossier_d_exemple_sans_son_ino_construit_le_paquet(self):
        # Folder renamed without its .ino: the old folder is gone (ignored), the
        # new one exists at head without Renomme.ino. The package build must run
        # (and fail in the PR) instead of the selection skipping it.
        ancien = self.racine / "lib/a/examples/A1"
        (self.racine / "lib/a/examples/Renomme").mkdir()
        (ancien / "A1.ino").rename(self.racine / "lib/a/examples/Renomme/A1.ino")
        ancien.rmdir()
        paquet = dict(RIEN, compiler=True)
        self.assertEqual(
            self.sel(["lib/a/examples/A1/A1.ino", "lib/a/examples/Renomme/A1.ino"]), paquet
        )
        # Same when the .ino is deleted but other files keep the folder alive.
        write(self.racine, "lib/b/examples/B1/aide.h", "")
        (self.racine / "lib/b/examples/B1/B1.ino").unlink()
        self.assertEqual(self.sel(["lib/b/examples/B1/B1.ino"]), paquet)

    def test_en_tete_inconnu_ou_parapluie(self):
        for inc in ("SteamiZ.h", "STeaMi.h"):
            with self.subTest(inc):
                write(self.racine, "lib/c/examples/C2/C2.ino", f"#include <{inc}>\n")
                r = self.sel(["lib/d/src/SteamiD.h"])
                self.assertEqual(r["exemples"], ["lib/c/examples/C2"])

    def test_inclusion_ancree_en_debut_de_ligne(self):
        # Une chaine ou un commentaire qui cite #include ne cree pas de dependance.
        write(
            self.racine,
            "lib/c/examples/C2/C2.ino",
            '// #include <SteamiD.h>\nconst char* s = "voir #include <SteamiD.h>";\n',
        )
        self.assertNotIn("lib/c/examples/C2", self.sel(["lib/d/src/SteamiD.h"])["exemples"])

    def test_inclusion_avec_espaces(self):
        write(self.racine, "lib/c/examples/C2/C2.ino", '  #  include "SteamiD.h"\n')
        self.assertEqual(self.sel(["lib/d/src/SteamiD.h"])["exemples"], ["lib/c/examples/C2"])

    def test_ligne_de_commande(self):
        def git(*args):
            return subprocess.run(
                [
                    "git",
                    "-c",
                    "user.name=t",
                    "-c",
                    "user.email=t@t",
                    "-c",
                    "commit.gpgsign=false",
                    "-C",
                    str(self.racine),
                    *args,
                ],
                check=True,
                capture_output=True,
                text=True,
            )

        git("init", "-q")
        git("add", "-A")
        git("commit", "-q", "-m", "base")
        write(self.racine, "lib/c/examples/C1/C1.ino", "#include <SteamiC.h>\n// change\n")
        git("add", "-A")
        git("commit", "-q", "-m", "change")
        proc = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--base",
                "HEAD~1",
                "--head",
                "HEAD",
                "--racine",
                str(self.racine),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            json.loads(proc.stdout),
            {
                "compiler": True,
                "tout": False,
                "exemples": ["lib/c/examples/C1"],
                "pio_exemples": ["lib/c/examples/C1"],
                "pilotes": ["c"],
            },
        )

    def _deplacer_et_selectionner(self, source: str, cible: str) -> dict:
        def git(*args):
            return subprocess.run(
                ["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false"]
                + ["-C", str(self.racine), *args],
                check=True,
                capture_output=True,
                text=True,
            )

        write(self.racine, "platformio.ini", "[env]\n")
        write(self.racine, "tests/native/.keep", "")
        git("init", "-q")
        git("add", "-A")
        git("commit", "-q", "-m", "base")
        (self.racine / cible).parent.mkdir(parents=True, exist_ok=True)
        git("mv", source, cible)
        git("commit", "-q", "-m", "move")
        proc = subprocess.run(
            [sys.executable, str(SCRIPT), "--base", "HEAD~1", "--head", "HEAD"]
            + ["--racine", str(self.racine)],
            check=True,
            capture_output=True,
            text=True,
        )
        return json.loads(proc.stdout)

    def test_renommage_d_un_source_vers_un_chemin_sans_effet(self):
        # Without --no-renames, git prints only the new path and the old one is lost.
        r = self._deplacer_et_selectionner("lib/a/src/SteamiA.cpp", "tests/native/SteamiA.cpp")
        self.assertTrue(r["compiler"])
        self.assertIn("a", r["pilotes"])

    def test_renommage_d_un_fichier_partage_vers_un_chemin_sans_effet(self):
        r = self._deplacer_et_selectionner("platformio.ini", "docs/platformio.ini")
        self.assertTrue(r["tout"])

    def test_ligne_de_commande_git_en_echec(self):
        proc = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--base",
                "inconnu1",
                "--head",
                "inconnu2",
                "--racine",
                str(self.racine),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(proc.returncode, 2)


class ArbreReelTest(unittest.TestCase):
    def setUp(self) -> None:
        self.mod = load_module()

    def test_depends_couvre_les_inclusions(self):
        noms = {}
        for props in ROOT.glob("lib/*/library.properties"):
            m = re.search(r"^name=(.+)$", props.read_text(encoding="utf-8"), re.M)
            noms[m.group(1).strip()] = props.parent.name
        inclusions = self.mod.dependances_directes(ROOT)
        for props in sorted(ROOT.glob("lib/*/library.properties")):
            pilote = props.parent.name
            m = re.search(r"^depends=(.*)$", props.read_text(encoding="utf-8"), re.M)
            declares = {
                noms[n.strip()] for n in (m.group(1).split(",") if m else []) if n.strip() in noms
            }
            inclus = inclusions.get(pilote, set())
            for q in sorted(inclus - declares):
                self.fail(f"{pilote} inclut {q} absent de depends=")
            for q in sorted(declares - inclus):
                self.fail(f"{pilote} declare {q} dans depends= sans l'inclure")

    def test_inclusions_steami_connues(self):
        table = self.mod.table_en_tetes(ROOT)
        inconnus = []
        for chemin in ROOT.glob("lib/**/*"):
            if chemin.suffix not in (".h", ".cpp", ".ino"):
                continue
            for nom in self.mod.inclusions(chemin.read_text(encoding="utf-8", errors="replace")):
                if re.match(r"(?i)steami.*\.h$", nom) and nom not in table:
                    inconnus.append(f"{chemin.relative_to(ROOT)}: {nom}")
        self.assertEqual(inconnus, [])

    def test_tout_couvre_chaque_exemple(self):
        r = self.mod.selectionner(["platformio.ini"], ROOT)
        self.assertEqual(
            r["exemples"],
            sorted(
                str(p.parent.relative_to(ROOT))
                for p in ROOT.glob("lib/*/examples/*/*.ino")
                if p.stem == p.parent.name
            ),
        )
        self.assertEqual(len(r["pio_exemples"]), len({e.split("/")[1] for e in r["exemples"]}))


if __name__ == "__main__":
    unittest.main()
