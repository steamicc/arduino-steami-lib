# Arduino Library Manager Package Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish every implemented STeaMi driver as one `STeaMi` library in the Arduino Library Manager, built and checked in CI and pushed automatically to `steamicc/arduino-steami` on each release.

**Architecture:** A stdlib-only Python script assembles the package from `lib/*/` into a directory. A reusable workflow builds it, lints it and compiles every example with arduino-cli; the PR CI and the publish workflow both call it. The publish workflow, triggered by `v*` tags or by hand, pushes the checked package to the distribution repository and attaches a ZIP to the source release.

**Tech Stack:** Python 3 (stdlib, `unittest`), GNU Make, GitHub Actions (`arduino/arduino-lint-action@v2`, `arduino/setup-arduino-cli@v2`, `tibdex/github-app-token@v2`), arduino-cli, STM32duino core `STMicroelectronics:stm32`.

**Spec:** `docs/superpowers/specs/2026-11-24-arduino-package-design.md`

## Global Constraints

- Library name `STeaMi`; `library.properties` exactly as in the spec, `version` = release version without the `v`.
- Excluded drivers: `hts221`. Drivers whose `src/` has no `.h`/`.cpp` are skipped.
- Example path in the package: `examples/<DRIVER DIR UPPER-CASED>/<Example>/`; BLE examples come from `lib/ble/examples/` as `examples/BLE/…`.
- FQBN `STMicroelectronics:stm32:STeaMi`; board package URL `https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json`.
- Distribution repository `steamicc/arduino-steami`, branch `main`, tag `vX.Y.Z`, commit message `Release vX.Y.Z`.
- ZIP asset name `STeaMi-X.Y.Z.zip`, top-level folder `STeaMi/`.
- App token from secrets `RELEASE_APP_ID` / `RELEASE_APP_SECRET` (same as `release.yml`).
- No file may contain the phrases rejected by `git-precommit-checks.config.js` (see its third rule).
- Commit messages follow commitlint (`type(scope): Subject.`), English, ending with a period, like the existing history.

## Review Focus

1. A driver whose `src/` holds only `.gitkeep` → skipped, no empty folder or include for it (test `test_placeholder_driver_skipped`).
2. An example folder holding extra files (`BleIndoorMap.md`, a local `.h`) → copied alongside the `.ino` (test `test_example_extra_files_copied`).
3. A stray file directly in `examples/` (`lib/mcp23009e/examples/.gitkeep`) → ignored, not an example (test `test_stray_file_in_examples_ignored`).
4. `--version v0.25.0` or an empty/garbled version → rejected with a clear error (test `test_version_must_be_semver`).
5. Re-running the publish for a tag that already exists in `arduino-steami` → no new commit, no error, release and ZIP still ensured (checked by the `dry_run` manual run in Task 4).

---

### Task 1: Package builder script and `make package`

**Files:**
- Create: `scripts/build-arduino-package.py`
- Create: `scripts/arduino-package/README.md` (README of the distribution repository, `{version}` placeholder)
- Create: `scripts/tests/test_build_arduino_package.py`
- Modify: `Makefile` (new `package` and `test-tooling` targets in the Build / Testing sections)
- Modify: `.gitignore` (add `build/`)

**Interfaces:**
- Produces: `python3 scripts/build-arduino-package.py --version X.Y.Z --out DIR` (exit 0 on success, 1 with a message on `PackageError`); in-module `build_package(repo_root: Path, out_dir: Path, version: str, exclude: frozenset[str] = EXCLUDED_DRIVERS) -> None`, `class PackageError(Exception)`, `EXCLUDED_DRIVERS = frozenset({"hts221"})`; `make package` → `build/arduino-package/` with version from `git describe --tags --abbrev=0` minus the leading `v`, `0.0.0` when there is no tag; `make test-tooling` runs the unit tests.

- [ ] **Step 1: Write the failing tests** in `scripts/tests/test_build_arduino_package.py` (`unittest`, each test builds a fixture `lib/` tree in a `tempfile.TemporaryDirectory`, plus a `LICENSE` and the README template, and loads the script with `importlib.util.spec_from_file_location` since its name has dashes):
  - `test_flattens_driver_sources`: drivers `aaa` (`AAA.h`, `AAA.cpp`, `AAA_const.h`) and `bbb` → all five files at `out/src/`.
  - `test_placeholder_driver_skipped`: `lib/ccc/src/.gitkeep` only → no `CCC*` file, no `CCC` example folder, `STeaMi.h` does not mention `ccc`.
  - `test_excluded_driver_skipped`: `lib/hts221/src/HTS221.h` → absent from `src/` and `examples/`.
  - `test_umbrella_header`: `out/src/STeaMi.h` contains `#include "AAA.h"` and `#include "BBB.h"`, sorted, and no `_const.h` include.
  - `test_library_properties`: parsed `out/library.properties` has `name=STeaMi`, `version=1.2.3`, `architectures=stm32`, `depends=STM32duinoBLE`, `includes=STeaMi.h`, `category=Sensors`, `url=https://github.com/steamicc/arduino-steami-lib`.
  - `test_examples_grouped_by_component`: `lib/wsen-pads/examples/altitude/altitude.ino` → `out/examples/WSEN-PADS/altitude/altitude.ino`; `lib/ble/examples/BleBeacon/BleBeacon.ino` → `out/examples/BLE/BleBeacon/BleBeacon.ino`.
  - `test_example_extra_files_copied`: `BleIndoorMap/BleIndoorMap.md` copied next to the `.ino`.
  - `test_stray_file_in_examples_ignored`: `lib/aaa/examples/.gitkeep` → no `out/examples/AAA/.gitkeep`.
  - `test_example_name_mismatch_rejected`: `lib/aaa/examples/Foo/foo.ino` → `PackageError` naming `Foo`.
  - `test_source_collision_rejected`: `lib/aaa/src/X.h` and `lib/bbb/src/X.h` → `PackageError` naming `X.h`.
  - `test_keywords_concatenated`: `lib/aaa/keywords.txt` and `lib/bbb/keywords.txt` lines both present in `out/keywords.txt`.
  - `test_license_and_readme`: `out/LICENSE` equals the fixture `LICENSE`; `out/README.md` contains `1.2.3`.
  - `test_out_dir_rebuilt`: a pre-existing `out/stale.txt` is gone after the build.
  - `test_version_must_be_semver`: `v1.2.3`, `""`, `1.2` → `PackageError`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest discover -s scripts/tests -v`
Expected: errors — `scripts/build-arduino-package.py` does not exist.

- [ ] **Step 3: Implement `scripts/build-arduino-package.py`**

SPDX header like `scripts/test-examples.py`. Version regex `^\d+\.\d+\.\d+$`. Drivers = sorted `lib/*/` with at least one `*.h`/`*.cpp` in `src/`, minus `exclude`; example sources = those drivers plus `lib/ble`. An example is a sub-directory of `examples/` that must contain `<dirname>.ino`. `STeaMi.h`: SPDX line, `#pragma once`, one-line comment, then the sorted includes. `library.properties`: the spec's block verbatim with the version substituted. README from `scripts/arduino-package/README.md` with `str.format(version=…)`. `main(argv: list[str] | None = None) -> int` with `argparse`.

`scripts/arduino-package/README.md` (English) states: generated from `steamicc/arduino-steami-lib` vX — do not edit here, issues and PRs go upstream; install with the Library Manager (search "STeaMi"); select **STeaMi Board** in *STM32 MCU based boards*; sensors are on the internal I²C bus (`TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);`); link to the wiki `https://wiki.steami.cc`.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python3 -m unittest discover -s scripts/tests -v`
Expected: 14 tests, OK.

- [ ] **Step 5: Add the Make targets and build the real package**

`package` (in `# --- Build ---`, help text `Build the Arduino Library Manager package into build/arduino-package`), `test-tooling` (in `# --- Testing ---`, `Run the Python tooling unit tests`); add `test-tooling` to the `ci` target.

Run: `make package && ls build/arduino-package build/arduino-package/examples`
Expected: `library.properties  LICENSE  README.md  examples  keywords.txt  src`; example folders `APDS9960 BLE ISM330DL LIS2MDL MCP23009E SSD1327 WSEN-HIDS WSEN-PADS`; no `HTS221`.

- [ ] **Step 6: Compile every example of the real package locally with arduino-cli**

With an isolated arduino-cli (STM32 core 3.x and STM32duinoBLE installed), copy `build/arduino-package` to `<user>/libraries/STeaMi` and compile each `examples/*/*/`.
Expected: 40 examples (44 minus the 4 HTS221 ones), all compile. Fix any failing example in its `lib/<driver>/examples/` source, in a separate `fix(<driver>)` commit.

- [ ] **Step 7: Commit**

```bash
git add scripts/build-arduino-package.py scripts/arduino-package/README.md scripts/tests/test_build_arduino_package.py Makefile .gitignore
git commit -m "feat(tooling): Build a single STeaMi library for the Arduino Library Manager."
```

---

### Task 2: Example compilation script for the package

**Files:**
- Create: `scripts/compile-package-examples.sh`

**Interfaces:**
- Consumes: package directory from Task 1.
- Produces: `scripts/compile-package-examples.sh PKG_DIR` — expects `arduino-cli` on `PATH` with the STM32 core and STM32duinoBLE installed; installs `PKG_DIR` as `$(arduino-cli config get directories.user)/libraries/STeaMi` (replacing any previous copy), compiles every `PKG_DIR/examples/*/*/` with FQBN `STMicroelectronics:stm32:STeaMi`, prints one `PASS`/`FAIL <example>` line each and a `Total: N | Passed: P | Failed: F` summary, exits 1 if any failed.

- [ ] **Step 1: Write the script** (bash, `set -euo pipefail`, SPDX header, no glob that aborts on no match: use `find … -mindepth 2 -maxdepth 2 -type d | sort`).

- [ ] **Step 2: Verify on a broken example**

Copy the package to a temp dir, append `#error broken` to one example, run the script on it.
Expected: that example `FAIL`, exit code 1.

- [ ] **Step 3: Verify on the real package**

Run: `scripts/compile-package-examples.sh build/arduino-package`
Expected: `Total: 40 | Passed: 40 | Failed: 0`, exit 0.

- [ ] **Step 4: Commit**

```bash
git add scripts/compile-package-examples.sh
git commit -m "tooling: Compile every example of the Arduino package with arduino-cli."
```

---

### Task 3: Reusable package check workflow, wired into PR CI

**Files:**
- Create: `.github/workflows/arduino-package-check.yml` (`on: workflow_call`, input `version: string`, required)
- Create: `.github/workflows/arduino-package.yml` (`on: pull_request` and `push` to `main`, calls the reusable workflow with version `0.0.0`)

**Interfaces:**
- Consumes: Task 1 CLI, Task 2 script.
- Produces: reusable workflow `./.github/workflows/arduino-package-check.yml` with input `version`; uploads artifact `arduino-package` (the package directory) on success. Task 4 calls it and downloads that artifact.

- [ ] **Step 1: Write `arduino-package-check.yml`**: one job on `ubuntu-latest`: checkout; `python3 -m unittest discover -s scripts/tests`; `python3 scripts/build-arduino-package.py --version ${{ inputs.version }} --out build/arduino-package`; `arduino/arduino-lint-action@v2` with `path: build/arduino-package`, `library-manager: submit`, `compliance: strict`, `project-type: library`; `arduino/setup-arduino-cli@v2`; cache `~/.arduino15` keyed on the core version; `arduino-cli core update-index` and `core install STMicroelectronics:stm32` with the board manager URL; `arduino-cli lib install STM32duinoBLE`; `scripts/compile-package-examples.sh build/arduino-package`; `actions/upload-artifact@v4` name `arduino-package`.

- [ ] **Step 2: Write `arduino-package.yml`** calling it (`uses: ./.github/workflows/arduino-package-check.yml`, `with: version: 0.0.0`).

- [ ] **Step 3: Validate the YAML**

Run: `python3 -c "import yaml,sys;[yaml.safe_load(open(f)) for f in sys.argv[1:]]" .github/workflows/arduino-package*.yml` (or `actionlint` if available)
Expected: no error.

- [ ] **Step 4: Commit, push the branch, open the PR**

```bash
git add .github/workflows/arduino-package-check.yml .github/workflows/arduino-package.yml
git commit -m "ci: Check the Arduino package on every pull request."
git push -u origin feat/arduino-package
```
Open the PR (template of the repository, `Closes #221`, `Closes #62`, refs #224).

- [ ] **Step 5: Verify in CI**

Run: `gh pr checks <PR>` until done.
Expected: the `arduino-package` check passes; arduino-lint reports no error. If arduino-lint `strict` fails on warnings that cannot be fixed in the package, lower to `compliance: specification` and note why in the PR.

---

### Task 4: Publish workflow

**Files:**
- Create: `.github/workflows/publish-arduino-package.yml`
- Modify: `README.md` (section "Arduino IDE": install from the Library Manager once registered; ZIP from the releases; `make package` for contributors)

**Interfaces:**
- Consumes: reusable workflow and artifact `arduino-package` from Task 3.
- Produces: on each `v*` tag, the content of `steamicc/arduino-steami` at tag `vX.Y.Z`, its GitHub release, and the `STeaMi-X.Y.Z.zip` asset on this repository's release.

- [ ] **Step 1: Write the workflow**:
  - `on: push: tags: ['v*']` and `workflow_dispatch` with input `dry_run` (boolean, default `false`). There is no `tag` input: manual recovery runs the workflow on the tag (`--ref vX.Y.Z`).
  - job `resolve`: output `tag` (`github.ref_name`, failing unless `github.ref_type == 'tag'`) and `version` (tag without `v`, validated `^v[0-9]+\.[0-9]+\.[0-9]+$`).
  - job `check`: `uses: ./.github/workflows/arduino-package-check.yml` with the resolved version. Note: the reusable workflow checks out the triggering ref, which is the tag, and has no `ref` input (CodeQL flags running code checked out from a dispatch input).
  - job `publish` (`needs: [resolve, check]`, `permissions: contents: write`): download artifact; generate App token (`repositories: arduino-steami,arduino-steami-lib`); clone `steamicc/arduino-steami` (handle an empty repository: no `main` yet); if `git ls-remote --tags origin vX.Y.Z` finds it, skip the push; else replace all content but `.git` with the package, commit as `semantic-release-updater[bot]`, tag, push `main` and the tag (skipped when `dry_run`); `gh release view vX.Y.Z -R steamicc/arduino-steami || gh release create …` with notes linking `https://github.com/steamicc/arduino-steami-lib/releases/tag/vX.Y.Z`; build `STeaMi-X.Y.Z.zip` (top folder `STeaMi/`) and `gh release upload vX.Y.Z STeaMi-X.Y.Z.zip --clobber -R steamicc/arduino-steami-lib`. With `dry_run`, print what would be pushed and stop before any write.

- [ ] **Step 2: Validate the YAML** (same command as Task 3 Step 3). Expected: no error.

- [ ] **Step 3: Update `README.md`** with the "Arduino IDE" section.

- [ ] **Step 4: Commit and push**

```bash
git add .github/workflows/publish-arduino-package.yml .github/workflows/arduino-package-check.yml README.md
git commit -m "ci: Publish the Arduino package to steamicc/arduino-steami on release."
git push
```

- [ ] **Step 5: After merge, verify the first publication and idempotence**

The `feat(tooling)` commit makes the merge release `v0.25.0`, whose tag triggers the first real publication (the maintainer must have checked the ruleset bypass before merging).
Run: `gh run list --workflow publish-arduino-package.yml --limit 1` until done, then `git ls-remote --tags https://github.com/steamicc/arduino-steami` and `gh release view v0.25.0 -R steamicc/arduino-steami-lib --json assets`.
Expected: run green; tag `v0.25.0` in `arduino-steami`; asset `STeaMi-0.25.0.zip` attached.
Then run: `gh workflow run publish-arduino-package.yml --ref v0.25.0` and check that `arduino-steami` has no new commit and the run is green (Review Focus 5). `dry_run=true` remains available for later diagnosis.

---

### Task 5: Registration and wiki (after the first publication)

**Files:**
- Modify (wiki repository `steamicc/wiki_steami`): `docs/software/arduino/bibliotheques.md`, `docs/software/arduino/capteurs.md` (example paths **Fichier → Exemples → STeaMi → …**), via its own issue and PR.

- [ ] **Step 1:** Maintainer opens the PR on `arduino/library-registry` adding `https://github.com/steamicc/arduino-steami` to `repositories.txt`.
- [ ] **Step 2:** Once `arduino-cli lib search STeaMi` finds it, update the wiki: install from the Library Manager, ZIP as the offline alternative, remove the interim notice. Build the wiki (`npm run build`). Expected: success, no broken link.
