# Arduino Library Manager package — design

- Date: 2026-11-24
- Issue: #221
- Status: draft, awaiting approval

## Intent

Teachers and students program the STeaMi with the Arduino IDE. Today they install the drivers by copying `lib/<driver>/` folders into their `libraries/` folder by hand (documented as an interim procedure in the wiki). The goal is a one-click install: **Tools → Manage Libraries → search "STeaMi" → Install**, which brings every driver and its examples.

## Goals

- One library named **`STeaMi`** in the Arduino Library Manager.
- Published automatically by every release of this repository, with the same version number. No manual step per release.
- Examples browsable in the IDE, grouped by component: **File → Examples → STeaMi → LIS2MDL → DigitalCompass**.
- The package is built and validated in CI on every PR (arduino-lint, compilation of every example with the Arduino IDE toolchain), so a release cannot publish a broken package.

## Non-goals

- Changing the development layout (`lib/<driver>/`, PlatformIO, Makefile, tests).
- A helper that creates the internal I²C bus and ready-made driver instances (`STeaMi.h` only aggregates includes for now; the helper is a separate issue).
- New drivers (BME280, GC9A01) and PlatformIO registry publication.

## Anti-goals

- Shipping placeholder drivers or anything that does not compile in the Arduino IDE (STM32duino core 3.x, which differs from the PlatformIO core 2.12: `.ino` prototype generation, no `wiring_private.h`).
- Any per-release manual work.
- Developers editing the distribution repository by mistake.

## Constraints

- Library Manager requires `library.properties` at the **repository root** and one Git tag per release; `name` is permanent once indexed; `version` must change on every tag; no symlinks, no `.exe`, no `.development` file.
- With the 1.5 recursive layout, only `src/` itself is on the include path: user sketches write `#include <LIS2MDL.h>`, so headers must sit at `src/` root.
- The release workflow authenticates with the `semantic-release-updater` GitHub App (installed on all `steamicc` repositories, `contents: write`). It already pushes directly to `main` here, so it is expected to bypass the organization ruleset on the new repository too (to confirm on the first release).
- Pre-commit checks reject the phrases listed in `git-precommit-checks.config.js` in any file.

## Approaches

1. **Generated distribution repository** `steamicc/arduino-steami` (**chosen**). The release workflow assembles the package and pushes it, with the same tag, to a dedicated repository registered in the Library Manager. This repository is untouched.
2. Restructure this repository into a single root library. Rejected: rewrites the Makefile, PlatformIO setup, tests and CI, and loses the per-driver layout, for no user-visible gain.
3. Orphan branch with its own tags in this repository. Rejected: the indexer would also scan the `v*` tags of `main` (not valid libraries), and the repository page would not look like a library.

## Design

### Distribution repository

`steamicc/arduino-steami` (created, empty, public). Generated content only. Its README states that it is generated and that issues and PRs go to `arduino-steami-lib`. Each release is one commit `Release vX.Y.Z` on `main`, the tag `vX.Y.Z`, and a GitHub release linking to the source release notes.

### Package layout

```text
library.properties
README.md
LICENSE                      (copied from this repository, GPL-3.0)
keywords.txt                 (concatenation of lib/*/keywords.txt for included drivers)
src/STeaMi.h                 (includes every public driver header)
src/<every .h/.cpp of the included drivers, flattened>
examples/<COMPONENT>/<Example>/<Example>.ino   (+ any other file of the sketch folder)
```

- **Included drivers**: every `lib/<driver>/` whose `src/` contains sources, minus an exclude list kept in the build script. Initial exclude list: `hts221` (legacy sensor, mounted on no production board). Placeholders (empty `src/`) are skipped automatically.
- **Examples**: every `lib/<driver>/examples/<Example>/` of the included drivers, plus `lib/ble/examples/` (no driver sources). `<COMPONENT>` is the upper-cased driver directory name (`ISM330DL`, `WSEN-PADS`, `MCP23009E`, `BLE`…).
- The build fails on a file-name collision in `src/` or on an example whose folder and `.ino` names differ.

### library.properties

```properties
name=STeaMi
version=<release version>
author=STeaMi contributors
maintainer=STeaMi contributors
sentence=Drivers for the sensors, buttons, battery gauge and display of the STeaMi board.
paragraph=Accelerometer and gyroscope (ISM330DL), magnetometer (LIS2MDL), distance (VL53L1X), D-PAD (MCP23009E), battery gauge (BQ27441), pressure, humidity and light sensors and OLED display of the V1a/V1b boards. Select the "STeaMi Board" in the STM32 MCU based boards package.
category=Sensors
url=https://github.com/steamicc/arduino-steami-lib
architectures=stm32
depends=STM32duinoBLE
includes=STeaMi.h
```

`depends=STM32duinoBLE` makes the IDE offer to install BLE support together with the package, so the BLE examples work out of the box. The per-driver `library.properties` stay for PlatformIO development; their `version` fields are not used by the package.

### Version

The package version is the version computed by semantic-release for this repository. The first published version is the first release after this work is merged (expected `0.25.0`).

### Build script

`scripts/build-arduino-package.py --version X.Y.Z --out <dir>`: Python standard library only, deterministic, rebuilds `<dir>` from scratch. `make package` wraps it, with the version taken from `git describe` (or `0.0.0` without a tag) and the output in `build/arduino-package/`.

### CI on pull requests

A `package` job (in `build.yml` or its own workflow) runs on every PR and on `main`:

1. `make package`;
2. `arduino/arduino-lint-action` on the output, `compliance: strict`, with `library-manager` chosen at run time from the Library Manager index (`scripts/library-manager-mode.sh`): `submit` while no library named exactly `STeaMi` is indexed, `update` once it is (submit would fail on LP017 and update on LP018 in the other case). If the index cannot be fetched or parsed the job fails rather than guess;
3. install arduino-cli, the STM32duino core (cached) and STM32duinoBLE, copy the package into the sketchbook `libraries/`, and compile every example with FQBN `STMicroelectronics:stm32:STeaMi`.

This also covers #62 (arduino-lint) and #224 (compile with the Arduino IDE toolchain) for everything users actually get.

### Release

`release.yml` is unchanged. Publishing is a separate workflow, `publish-arduino-package.yml`, so it can be retried without re-running semantic-release:

- **Triggers**: `release` of type `published` (semantic-release creates the release with the App token, which does trigger workflows; a `push` of the tag would not, because the tag commit carries `[skip ci]`) and `workflow_dispatch` (with an optional `dry_run` input), for manual recovery: the workflow is run on the tag itself (`--ref vX.Y.Z`) and refuses any other ref, so the tested code is always the tagged code.
- **Gate**: it checks out the tag, builds the package with the tag's version, runs arduino-lint and compiles every example with the Arduino IDE toolchain (same steps as the PR job). Nothing is published if any of this fails: the source release exists, the Library Manager does not get that version, and the fix ships in the next version. This is what guarantees "a release cannot publish a broken package", including when two PRs that pass separately break `main` together.
- **Publish** (each step idempotent, so a re-run after a partial failure is safe):
  1. clone `steamicc/arduino-steami` with the App token; if tag `vX.Y.Z` already exists there, skip to step 3;
  2. replace its whole content (except `.git`) by the package, commit `Release vX.Y.Z`, tag `vX.Y.Z`, push `main` and the tag;
  3. create the GitHub release `vX.Y.Z` in `arduino-steami` unless it exists;
  No ZIP is uploaded to this repository's release (releases are immutable, so uploading after semantic-release publishes fails). For offline installation, the "Source code (zip)" of the `steamicc/arduino-steami` release has the library at its root and installs with **Sketch → Include Library → Add .ZIP Library**.
- **Recovery**: if publishing fails (for example the ruleset blocks the App), fix the cause and run the workflow manually on the tag: `gh workflow run publish-arduino-package.yml --ref vX.Y.Z`. Skipping a version in the Library Manager is acceptable.

Before merging, the maintainer checks that the `semantic-release-updater` App may push and tag on `arduino-steami` (organization ruleset bypass list), so the first release goes through.

### Registration and documentation

- Once the first tag exists in `arduino-steami`, open a PR on `arduino/library-registry` adding `https://github.com/steamicc/arduino-steami` to `repositories.txt` (by the maintainer, under their account).
- Once indexed, update the wiki page *Bibliothèques STeaMi*: install from the Library Manager, new example paths; keep the "Source code (zip)" of the `arduino-steami` release as the offline alternative.

## Risks

- The organization ruleset may block the App's push or tag on the new repository. Mitigation: the first release shows it; then add the App to the bypass list.
- All drivers are compiled into every sketch that includes one header: possible global symbol clashes and longer builds. Mitigation: the CI compiles every example from the package; the linker drops unused code.
- The name is permanent: `STeaMi` is free today (checked with `arduino-cli lib search`).

## Left to the builder

CI caching details, the exact README text of the distribution repository, the job layout (new workflow or extra job), and the order of the PRs.
