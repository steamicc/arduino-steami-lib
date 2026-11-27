# STeaMi display, delivery 4b-font (the V1c fonts) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Draw all text of the 240 px panel (V1c) in Spleen 2.1.0 bitmap fonts: widgets (8x16 text, 6x12 labels, bold 12x24/16x32 values) and primitives (`text()` in 8x16 × scale, `textSmall()` in 6x12 × scale). Add `textHeight()`. Keep every pixel of the 128 px panel identical.

**Architecture:**
- A stdlib Python generator, pinned to the Spleen tarball by sha256, writes the committed `SteamiFontSpleen.cpp` (one `const` object per face). With `--micropython`, it also writes the tested `spleen.py`, which is committed nowhere.
- `SteamiFont` gains an extended decoder (codes 32..159) and per-face lookups.
- `SteamiDisplay` gains one panel predicate and one row drawer; `text()` and `textSmall()` each pass their own 240 face to it.
- The `SteamiScreen` kernel keeps a one-byte face id in `Run`'s padding byte. It takes advances, ink rows and replication from the face; the vertical formulas keep 8f.
- `golden_layout.py` parses the tables and stays the oracle of every pinned 240 number.

**Tech Stack:** C++17 (PlatformIO `env:native`, `env:native_asan`, `env:steami`, STM32duino 3.1.0), Unity, Python 3 standard library, GitHub Actions.

**Spec:** docs/superpowers/specs/2026-11-26-steami-display-v1c-font-design.md (amending docs/superpowers/specs/2026-11-25-steami-display-design.md).
- Below, **the design** is the first file and **the spec** the second.
- The design is the binding authority. Read its sections 5.1 to 5.7, 6, 7, 9, 10 and 11 before your task.
- Section 8 is follow-up work. Only the `spleen.py` contract of 5.6 is produced and tested here.

Branch `feat/steami-display-4b-font`, built on `feat/steami-display-4b` (PR #277, still a draft) plus the design commits.
- Issue #280; the PR says "Closes #280".
- The PR opens as a draft stacked on #277, and is rebased onto `main` once #277 is merged.
- `tests.yml`, `lint.yml` and `check-commits.yml` run only for PRs into `main`, so until then every check runs locally.

## Global Constraints

From the design (verbatim):
- "Never change a pixel on 128 px. Never fetch a font at build time: the tables are committed."
- "No student-visible font choice. The only new public API is `textHeight()` (5.3); the other visible changes are values on 240: `maxChars()` and the height of `text()`."
- "No RAM copy of a table (all `const`, in flash), no heap, no widget frame above 64 B, and no new local in a frame that is already at 64 B (5.7)."
- "Never hand-edit a generated table; never ship Spleen data without its BSD-2 notice in the source **and** in the documentation of the Arduino package (5.6)."
- "No heap, no `String`, both cores (STM32duino 2.12 and 3.1.0), native tests without a board."
- "Geometry: polar points stay Q14 (`polarX/polarY`); only the cell placed around a point changes."
- "The library cannot know the release at compile time (no `STEAMI_BOARD_*` in a `.cpp`)."
- "each face blob is its own `const` object, and no table of faces that a primitive reaches points to the other blobs [...] **`text()` and `textSmall()` name their 240 face themselves** [...] no run-time flag selects between 8x16 and 6x12."
- "Primitives never use the 12x24 and 16x32 value sets, bold or the narrow '.' and ':'."
- "In a frame at 64 B (`value`, `subtitle`, `compass`) and in `gauge`'s, a changed call must not add a stack argument [...] prefer `const Run&` over extra scalars."
- "`glyphsAt` builds no `Run`."
- Fallback: "the task shrinks its locals; if that is impossible without breaking another spec rule, the builder stops and reports a spec deviation; a helper is never exempted."
- Licence: Spleen is "BSD-2-Clause (Copyright (c) 2018-2024, Frederic Cambus)".
- sha256 values:
  - tarball `spleen-2.1.0.tar.gz`: `8b47c56f1a6eb858fbcf9e34530557404b02fbb3455e38e64fb84473fd0c372f`;
  - 6x12: `f40e935bdd194976d30ef296920d5ccca29eb84bb7b3459a88fe7932fa8ba872`;
  - 8x16: `b2b05484ba4380c9c6854cc57637235a9fb039c65b5ba38a65289540f8fab7dc`;
  - 12x24: `eb690d1dd190e21ae31f800e63fe1188409e3d94a11da222196de0777ed8c9e8`;
  - 16x32: `3b6ae73ef1dd3c78bad873697c18b15df82d8e5c0258114fda401556e01d0e67`.
- Value key `" +-.:0123456789"`; table total 5 264 B.

Carried from 4a/4b (plans and ledger):
- **Setup.** The controller sets up the worktree: `.venv` and `node_modules` are symlinks to `/home/nedjar/sandbox/arduino-steami-lib/`'s.
  - Only run their tools: never `make setup`, `pip install`, or a write under another checkout.
  - Hooks do not run here, so every **Commit** step starts with **the commit check**, by hand, for each commit it makes:
    - `printf '%s\n\n%s\n' "<header>" "<body, if any>" | npx --no-install commitlint` passes (`commitlint.config.js`);
    - `make lint` passes when the commit touches a `.h`, `.cpp` or `.ino`.
- **Commits.** `type(scope): Sentence with a capital and a final period.`, at most 72 characters.
  - Scopes: `board`, `tests`, `tooling`, `ci`, `docs`.
  - No `!`, no "BREAKING CHANGE", no `[skip ci]`, no attribution lines, and never the phrase blocked by `git-precommit-checks.config.js`.
- **Style.** Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`, except the generated `SteamiFontSpleen.cpp` (BSD-2-Clause). Google style, indent 4, 100 columns.
- **clang-tidy** (`WarningsAsErrors: '*'`):
  - cast to `int32_t` (`size_t` for offsets) before multiplying;
  - no `uint8_t` loop variable against a wider bound;
  - `smaller`/`larger`, never unqualified `min`/`max`/`abs`;
  - no namespace-scope object with a constructor (face descriptors are constant-initialised aggregates).
- **Tests.** Build screens on the file-local `board` and `RUN_TEST` every new test. "A == B" means each is rendered alone on a fresh, cleared panel of the same size and mode, then compared.
- **Sizes and arithmetic.**
  - `sizeof(SteamiDisplay)` and `sizeof(SteamiScreen)` stay ≤ `3 * sizeof(void*)`: no new member, no cached face.
  - Rows and columns are computed in `int32_t` (`y + row * n`, `x + n`) and cast to `int16_t` after the subtraction; UBSan runs scale 255 and ±32768.
- **Hardware.** Never flash, open a serial port, touch `/dev/ttyACM*` or `/run/media`, or run `make test-hardware`. Only Task 12 uses the board, run by the controller.
- **Read-only references.**
  - Spleen release: `/tmp/claude-1000/-home-nedjar-sandbox-wiki-steami/1346e80f-1de2-4e4c-bae2-38b41b348575/scratchpad/dl-spleen/` (`spleen-2.1.0.tar.gz`, `spleen-2.1.0/`); read it with `python3 -I`.
  - The spike `…/scratchpad/spike-font/spike.py`, as an illustration only.
  - The mockup refs at `/home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups`.

Checks every task uses (`$SCRATCH` is your scratchpad, `ROOT` the worktree):
- **The 128 px check:**
  ```bash
  PPM="$SCRATCH/ppm"; rm -rf "$PPM"; mkdir -p "$PPM"
  STEAMI_PPM_DIR="$PPM" .venv/bin/pio test -e native -f native/test_steami_display -f native/test_steami_screen_mockups
  (cd "$PPM" && sha256sum -c "$ROOT/tests/native/test_steami_display/ppm-128.sha256")
  ```
  Expected: 14 `OK`. A failure means a 128 px pixel changed: fix the code. The list is never edited after Task 1.
- **The 128 literal check** (Tasks 4 to 7, before the commit): the hashes cover only the gallery and the 13 mockups; most 128 layouts are guarded by `test_steami_screen` assertions. Read `git diff -U0 fc9a7e9 -- tests/native/test_steami_screen/ tests/native/test_steami_display/` and confirm that no literal of a 128 branch (`size == 128`, `panel == 0`, the 128 half of a `forEachPanel` table) changed. Rewritten helpers return on 128 exactly what they returned in 4b. A re-pinned 128 literal is a bug in the code, never a new expectation.
- **The stack measurement:**
  ```bash
  SU="$SCRATCH/su"; rm -rf "$SU"
  PLATFORMIO_BUILD_DIR="$SU" PLATFORMIO_BUILD_FLAGS=-fstack-usage PLATFORMIO_SRC_DIR=lib/steami_board/examples/ScreenTemperature .venv/bin/pio run -e steami
  grep -h 'SteamiScreen::' $(find "$SU" -name 'SteamiScreen.cpp.su'); grep -h 'SteamiDisplay::' $(find "$SU" -name 'SteamiDisplay.cpp.su')
  ```
  - Bound to 64 B:
    - the widgets: `face` ×3, `watch`, `compass`, `gauge`, `title`, `subtitle`, `value`, `text`, `bar`;
    - the helpers holding a buffer, `Run`, `Region` or vertices: `pointer`, `gaugeLabel`, `drawGaugeLabel`, `fitGlyphs`, `placeValue`, `fitValue`, `drawValuePart`, `drawValueParts`, `linesFit`.
  - Recorded only: the other kernel helpers and the primitives' text frames (`text`, `textSmall`, `drawText`, `drawFaceText`, `faceGlyph`).
  - 4b's frames, for comparison: face 64, watch 48, compass 64, gauge 56, title 16, subtitle 64, value 64, text 56, bar 48, pointer 64, gaugeLabel 48, drawGaugeLabel 32, fitGlyphs 64, drawGlyphs 88, glyphsAt 40.
  - Every frame at 64 B in that list (`face`, `compass`, `subtitle`, `value`, `pointer`, `fitGlyphs`) gains no new live value: the design's "no new local in a frame that is already at 64 B" covers bound helpers too. For example, `fitGlyphs` does not compute n itself; `rowsChord` derives it from the `Run` (Task 5).
- **The golden script:** `python3 tests/native/test_steami_screen/golden_layout.py`, exit 0. It is run by hand; no CI job runs it.
- **The baseline tree:** `mkdir -p "$SCRATCH/base" && git archive fc9a7e9 | tar -x -C "$SCRATCH/base"`.
  - fc9a7e9 is the 4b tip; its code equals this branch's base.
  - Never take a baseline on `main`, which has no ScreenGauge and no 4b mockups.

## Decisions taken while planning (design section 12 leaves them to the builder)

- **D1 Face switch.** `bool SteamiDisplay::spleenPanel()` (protected, non-const) is `(width() + 64) / 128 == 2`, i.e. `k() == 2`.
  - It is recomputed on each call. `width()` is valid before `begin()` and is 0 without a backend.
  - The primitives, `textHeight()` and the kernel share it.
  - 2x1 and 96 px panels take 8x8; a 200 px panel takes Spleen.
- **D2 Face type** (`SteamiFont.h`, by hand):
  ```cpp
  struct SteamiFontFace {
      uint8_t w;            // cell width = advance
      uint8_t h;            // cell height
      uint8_t ascent;
      uint8_t glyphs;       // 128 (codes 32..159) or 15 (the value key)
      const uint8_t* data;  // rows top to bottom, (w + 7) / 8 bytes per row, MSB = leftmost column
  };
  extern const SteamiFontFace STEAMI_SPLEEN_6X12, STEAMI_SPLEEN_8X16, STEAMI_SPLEEN_12X24, STEAMI_SPLEEN_16X32;
  ```
  - Metrics: 6x12 `{6, 12, 9, 128}`, 8x16 `{8, 16, 12, 128}`, 12x24 `{12, 24, 19, 15}`, 16x32 `{16, 32, 26, 15}`.
  - Full sets are indexed by code − 32 (D5); value sets follow the key.
- **D3 Generated file** `lib/steami_board/src/SteamiFontSpleen.cpp`.
  - **Header.** Line 1 is `// SPDX-License-Identifier: BSD-2-Clause`, followed by:
    - the version, URL, tarball and four BDF sha256 values;
    - the generator path, with "regenerate with this script, never edit by hand";
    - the full BSD-2 notice and the format.
    - Each notice line becomes `("// " + line).rstrip()`: a blank LICENSE line is a bare `//`, never `// ` with a trailing space, which `make format-check` rejects outside the clang-format off region.
  - **Reproducible.** No date, no commit, no local path. ASCII only, LF line endings, a final newline.
  - **Body.** `#include "SteamiFont.h"`, then an anonymous namespace holding the four arrays between `// clang-format off` and `// clang-format on`:
    - `const uint8_t kSpleen6x12[1536] = {`, `kSpleen8x16[2048]`, `kSpleen12x24[720]`, `kSpleen16x32[960]`;
    - each glyph is a comment line (`// code 65 U+0041`) followed by one line per font row: its `0x..` bytes, then `//` and `#`/`.` art;
    - no `0x` or `};` appears inside a comment.
  - **Descriptors**, after the namespace, such as `const SteamiFontFace STEAMI_SPLEEN_8X16 = {8, 16, 12, 128, kSpleen8x16};`.
  - The file is never piped through clang-format.
- **D4 Generator** `scripts/gen-spleen-fonts.py`.
  - Usage: `[--tarball PATH] [--cache DIR] [--out PATH] [--micropython PATH] [--commit SHA]`.
  - Defaults, relative to `ROOT = Path(__file__).resolve().parent.parent` (the repository root, as in `check-footprint.py`), never to the current directory; the tests anchor their paths the same way:
    - cache `ROOT/.spleen-cache/`, one `.gitignore` line `/.spleen-cache/`;
    - out `ROOT/lib/steami_board/src/SteamiFontSpleen.cpp`;
    - commit `git rev-parse HEAD`, written only in `spleen.py`.
  - The download is `https://github.com/fcambus/spleen/releases/download/2.1.0/spleen-2.1.0.tar.gz` (never `archive/refs/tags`), through `urllib.request` with a generic User-Agent.
  - Every tarball is sha256-checked before use: downloaded, cached or `--tarball`.
  - `main()` calls `read_release(tarball, TARBALL_SHA256, BDF_SHA256)`, reading the module globals at call time (never through the defaults bound at definition). It then compares LICENSE with `NOTICE` before writing any output; on a mismatch it prints a `FontError` message naming `LICENSE` to stderr and returns 1.
  - `EXTRA` and `FOLD` are literals in the generator; it never reads `SteamiFont.cpp`. Only the tests read `kFolds`.
  - Members are read only through `extractfile()`: `spleen-2.1.0/spleen-WxH.bdf` and `spleen-2.1.0/LICENSE`. Never `extractall`; never `cp437/`.
  - No Makefile target: row 10 does not list the Makefile.
- **D5 Codes.**
  - 32..126 ASCII; 127 °; 128..158 À Â Ä Ç È É Ê Ë Î Ï Ô Ö Ù Û Ü à â ä ç è é ê ë î ï ô ö ù û ü ÿ (the order of `kFolds`); 159 Ÿ.
  - The 8x8/5x7 paths keep `steamiNextGlyph` (folded); every Spleen path uses `steamiNextCode`.
- **D6 Primitives.**
  - `text()` calls `drawFaceText(…, STEAMI_SPLEEN_8X16)` when `spleenPanel()`, else `drawText(…, false)`. `textSmall()` does the same with `STEAMI_SPLEEN_6X12` and `drawText(…, true)`.
  - `drawFaceText` advances `face.w · scale` and stops past the right edge.
  - `faceGlyph`, the one row drawer shared with the kernel:
    - one `span()` per run of set bits per font row, n×n blocks, never `pixel()`;
    - with `bold`, a run `[a, b]` of the window spans `[a·n, (b + 2)·n − 1]` from the window's left edge;
    - the generator guarantees the widened run stays in the window and the cell.
  - 8x8 and 5x7 keep `glyph()`.
- **D7 Kernel faces.**
  - Protected `enum class FaceId : uint8_t { NONE, F8X8, F5X7, F6X12, F8X16, F12X24, F16X32 };`.
  - `Run` gains `FaceId face = FaceId::NONE;` after `shrink`: still 16 B on ARM and 24 B on x86-64.
  - Every code that sets a Run's text and f sets `face`:
    - `text()`, `linesFit`, `subtitle`, `gaugeLabel`, and test Runs;
    - `placeValue`, on each f tried;
    - both parts of `drawValueParts`, including the `part = {…}` reassignment.
  - A NONE run inks and draws nothing.
  - `faceFor(s, ink, f)` on a non-Spleen panel: LABEL → F5X7, else F8X8. On a Spleen panel:
    - LABEL → F6X12;
    - TEXT → F8X16 at even f, NONE at odd f;
    - VALUE (Task 6), judged on the whole of s: a text with a glyph outside the key takes the TEXT rule;
    - a text made only of value glyphs (nullptr and "" included): f 1 NONE, 2 F8X16, 3 F12X24, 4 F16X32; above 4: F16X32 if 4 | f, else F12X24 if 3 | f, else F8X16 if f is even, else NONE.
  - Replication n:
    - f for F8X8 and F5X7;
    - f/2 for F6X12 and F8X16;
    - f/3 for F12X24;
    - f/4 for F16X32;
    - 1 for NONE.
  - Cell height: 8f for F8X8 and every TEXT/VALUE Spleen face; 7f for F5X7; 12·(f/2) for F6X12.
  - `glyphAdvance(ink, f)`:
    - on 128: 8f for TEXT and VALUE, 6f for LABEL;
    - on Spleen: 4f for TEXT, 6·(f/2) for LABEL; 4f for VALUE (any face) **from Task 6 on**. During the interim (D12) VALUE keeps 8f on every panel, because its face is still F8X8 (design 5.2: the advance is the face's cell width);
    - narrow VALUE '.'/':' take half.
  - Narrow window on Spleen faces: `[w/4, w/4 + w/2)`.
  - Bold: VALUE ink on F12X24 or F16X32.
  - The vertical formulas take `STEAMI_CELL_HEIGHT = 8` (new, `SteamiScreenConst.h`) instead of `CHAR_H`/`CHAR_W`, with the same numbers:
    - the `marginNs` floor;
    - `marginEw` (8f, **not** the advance);
    - the S row, `blockHeight`, `valueRegion` and the unit top.
  - After Task 7, `SteamiScreen.cpp` names neither constant.
- **D8 128 px hashes.**
  - `tests/native/test_steami_display/ppm-128.sha256` lists, in `sha256sum` format, `steami-display-gallery-128-gray.ppm` and the 13 `steami-mockup-*-128-gray.ppm`.
  - It is recorded in Task 1, before any drawing change.
  - The `display-sanitizers` job of `tests.yml` runs `sha256sum -c` on it.
- **D9 Golden script.**
  - New sections are titled `(4b-font Task N)`, in 4b's pattern: model, self-check asserts, prints.
  - Spleen is parsed from `FONT_CPP.with_name("SteamiFontSpleen.cpp")`, by array name and descriptor line.
  - Every pinned 240 number is printed.
  - The design's section 6 figures are asserted by the task that produces them.
- **D10 Known differences.**
  - The two 4b gauge-label entries on 240 are removed in Task 5, once the goldens show them clear. The 128 entries stay.
  - The spec has no program 04 entry, because 4b left it to the maintainer. Task 9 adds "04: the title and the subtitle paint over the ends of the gauge ring on 128 (V1a/V1b)", or rewrites 4b's entry to say that if one has landed by then.
- **D11 Release note.**
  - PR title (feat → minor): `feat(board): Draw V1c text in Spleen fonts, 16 px tall at scale 1.`
  - The repository squash-merges PRs and semantic-release builds `CHANGELOG.md` from the header only, so the PR title is the changelog line (design 5.3: "the changelog says so"). The PR text asks that the squash commit body keep the two sentences below; the commit bodies alone never reach the changelog.
  - Bodies are wrapped under 100 columns (commitlint `body-max-line-length`).
  - Task 4's commit body: "On the V1c, text() is now twice as tall at the same width (8x16 glyphs); textHeight() gives the line pitch on either screen."
  - Task 5's commit body: "maxChars() is 30 on the V1c."
- **D12 Interim.** From Task 5 to Task 6, values stay 8x8 on 240, exactly as in 4b, and every commit is green:
  - `faceFor(…, VALUE, f)` is F8X8 on both panels;
  - `glyphAdvance(Ink::VALUE, f)` is 8f on both panels ('.' and ':' 4f), so every 240 value width, fit and pin stays as in 4b;
  - `placeValue` already sets `value.face = faceFor(value.s, Ink::VALUE, value.f)` on each f tried (F8X8 here), so no value run is NONE;
  - `value()` keeps its floor at 1.
  - Task 6 switches the VALUE ladder of `faceFor` and the VALUE advance (4f on a Spleen panel) together.

## Review Focus

1. **Empty and nullptr strings in the new face code** (value label or unit, titles, subtitles, gauge labels).
   - Expected: no crash and no read past the string; nothing drawn for that part.
   - Test: `test_faces_accept_empty_strings` (Task 5).
2. **Characters outside the table on the V1c in widgets** (control bytes, U+007F, €, invalid UTF-8).
   - Expected: one '?' cell each, in the TEXT or LABEL face, never in a value face.
   - Tests: `test_unknown_characters_in_widgets` (Task 5), `test_unknown_characters_in_values` (Task 6).
3. **Scale 255 on the V1c** (16 × 255 px cells, bold `x + n`, the fit stepping through faceless f).
   - Expected: termination, nothing out of range under UBSan, nothing outside the circle, `textHeight(255) == 4080`.
   - Tests: `test_text_height` and the existing `textClippingCheck` (Task 4), `test_values_at_scale_255` (Task 6).
4. **Panels other than 128 and 240** (200 px, 96 px).
   - Expected: one predicate. On 200, `textHeight()` is 16, `text()` draws 8x16, and a widget's text equals `text()` at its cell. On 96 every one of them is 8x8.
   - Tests: `test_text_height` (Task 4), `test_one_font_per_panel` (Task 5).
5. **A backend replaced after use** (`board.end()`, then a 128 panel after drawing on 240).
   - Expected: `textHeight()` and `text()` follow the new panel.
   - Test: `test_text_follows_a_new_backend` (Task 4).

## Paths

Each task lists its files. The PR names the paths outside row 10, and why:
- `tests/native/test_steami_display/ppm-128.sha256`, the hash list;
- `scripts/tests/test_check_footprint.py`: tests follow the code they check (4b Ruling 7);
- `.github/workflows/tests.yml`, the 128 px hash step (design 7 "a test or CI step checks them", design 12); row 10's `.github/workflows/` names only the regeneration check (`lint.yml`).

The PR text also notes two exceptions to `CONTRIBUTING.md`, which is not edited: the generated `SteamiFontSpleen.cpp` carries `BSD-2-Clause`, not `GPL-3.0-or-later`, on line 1 (third-party data), and `lint.yml` gains the `spleen-fonts` job beside `make lint`.

`scripts/tests/test_steami_display_rules.py` and `test_steami_display_board` stay unchanged (design 10).

---

### Before Task 1: commit the plan

- [ ] **Step 0:** Once the maintainer has approved the plan, commit it alone:

```bash
git add docs/superpowers/plans/2026-11-27-steami-display-4b-font.md
git commit -m "docs(docs): Plan display delivery 4b-font, the V1c fonts."
```

---

### Task 1: Pin the 128 px PPMs before any drawing change

**Files:** Create `tests/native/test_steami_display/ppm-128.sha256`; modify `.github/workflows/tests.yml` (job `display-sanitizers`).

**Interfaces:**
- Consumes: the PPM writers `galleryOn` (test_steami_display) and `render()` (test_steami_screen_mockups), which write to `$STEAMI_PPM_DIR`.
- Produces: the hash list of D8.

- [ ] **Step 1: Record.** On the unchanged tree, render with the first two lines of the 128 px check, then run `(cd "$PPM" && sha256sum *-128-gray.ppm) > tests/native/test_steami_display/ppm-128.sha256`. Expected: 14 sorted lines without paths.
- [ ] **Step 2: Verify determinism.** Run the 128 px check, then `rm -rf .pio/build/native` and run it again. Expected: 14 `OK` both times.
- [ ] **Step 3: CI.** After "Upload the primitives gallery" (so the PPMs are uploaded even when a hash fails), add the step "Check the 128 px renders are unchanged":
  - it repeats the step-level `env: STEAMI_PPM_DIR: ${{ runner.temp }}/steami-display-ppm` (the env of "Run the display suites" is not shared);
  - `run: cd "$STEAMI_PPM_DIR" && sha256sum -c "$GITHUB_WORKSPACE/tests/native/test_steami_display/ppm-128.sha256"`.
  Locally, run `.venv/bin/pio test -e native_asan` with `STEAMI_PPM_DIR` set, then the same check. Expected: 14 `OK`, so `native_asan` renders equal `native`'s.
- [ ] **Step 4: Commit** (the commit check first)

```bash
git add tests/native/test_steami_display/ppm-128.sha256
git commit -m "test(tests): Pin the sha256 of every 128 px PPM."
git add .github/workflows/tests.yml
git commit -m "ci(ci): Check the 128 px PPM hashes after the sanitizer run."
```

---

### Task 2: Generator, tables, `spleen.py` and licence notices

**Files:**
- Create: `scripts/gen-spleen-fonts.py`, `scripts/tests/test_gen_spleen_fonts.py`, `lib/steami_board/src/SteamiFontSpleen.cpp` (generated).
- Modify:
  - `.gitignore`;
  - `lib/steami_board/src/SteamiFont.h` (D2 only);
  - `scripts/arduino-package/README.md`, `scripts/tests/test_build_arduino_package.py`;
  - `lib/steami_board/README.md` (a new `## Third-party fonts` before `## License`);
  - `.github/workflows/lint.yml`.

**Interfaces:**
- Consumes:
  - the Spleen release;
  - `kFolds` in `SteamiFont.cpp`, read **by the tests only** with `r"\{(0x[0-9A-Fa-f]{2}), '(.)'\}"` (as `golden_layout.py:34`); the generator holds `EXTRA` and `FOLD` as literals (D4);
  - the MIT notice, `SteamiFont.cpp` lines 7-29, read by the package test.
- Produces:
  - D2's type and D3's file;
  - generator module names, used by this task's tests:
    - `VERSION = "2.1.0"`, `URL`, `TARBALL_SHA256`, and `BDF_SHA256: dict[str, str]` (keys `"6x12"`, `"8x16"`, `"12x24"`, `"16x32"`);
    - `VALUE_KEY = " +-.:0123456789"`;
    - `EXTRA: str` and `FOLD: str`, 33 characters each (codes 127..159);
    - `FULL_CODES: tuple[int, ...]`, 128 code points: 32..126, 0xB0, the 31 letters of D5, 0x178;
    - `NOTICE: str`, the exact LICENSE text;
    - `class FontError(Exception)`;
    - frozen dataclasses:
      - `Glyph(dwidth: int, bbx: tuple[int, int, int, int], rows: tuple[int, ...])`, rows MSB-left and byte-padded;
      - `Bdf(w: int, h: int, ascent: int, descent: int, glyphs: dict[int, Glyph])`;
      - `Face(name: str, w: int, h: int, ascent: int, key: str | None, data: bytes)`;
    - `parse_bdf(text: str) -> Bdf`, `build_face(name: str, bdf: Bdf, value_set: bool) -> Face`;
    - `read_release(tarball: Path, tarball_sha256: str = TARBALL_SHA256, bdf_sha256: dict[str, str] = BDF_SHA256) -> tuple[dict[str, str], str]`, returning the four BDF texts and LICENSE;
    - `build_faces(bdfs: dict[str, str]) -> list[Face]`: 6x12 full, 8x16 full, 12x24 value, 16x32 value;
    - `render_cpp(faces: list[Face]) -> str`, `render_micropython(faces: list[Face], commit: str) -> str`, `main(argv: list[str] | None = None) -> int`;
  - `spleen.py` (the contract of design 5.6):
    - `VERSION`, `VALUE_KEY`, `EXTRA`, `FOLD`;
    - `F6X12`, `F8X16`, `F12X24`, `F16X32` = `(w, h, ascent, key, data)`, where key is `None` (full set) or `VALUE_KEY`, and data is one `bytes` literal (never `bytes.fromhex`, so a frozen module keeps it in flash);
    - a header with `# SPDX-License-Identifier: BSD-2-Clause`, the version, the sha256 values, the generator path, `--commit` and the notice.

- [ ] **Step 1: Write the failing generator tests.**
  - Load the script with `importlib.util.spec_from_file_location`, and set `sys.modules[spec.name]` before `exec_module` (the dataclass trap).
  - The helper `make_bdf(w, h, descent, glyphs={}, skip=(), dwidth=None, bbx=None) -> str` writes a BDF with every code point of `FULL_CODES` except `skip`.
  - Its glyphs are blank unless given as `#`/`.` rows, and `dwidth`/`bbx` override one glyph as a `(cp, value)` pair.

```python
def test_packs_rows_msb_left(self):
    # 6x12 U+0041 row 0 "#....#" -> data[33 * 12] == 0x84; 12x24 value '5' row 0 ".#........#." ->
    # 0x40, 0x20 at 10 * 24 * 2; len(data) 1536, 2048, 720, 960 (sum 5264)
def test_full_set_code_order(self):
    # index 95 U+00B0, 96 U+00C0, 116 U+00E9, 126 U+00FF, 127 U+0178; a 6x12 font without U+0178 gets
    # its 'Y' in slot 127; EXTRA == "°ÀÂÄÇÈÉÊËÎÏÔÖÙÛÜàâäçèéêëîïôöùûüÿŸ"
def test_value_set_follows_the_key(self):
    # glyph i of a 16x32 value face == the BDF glyph of VALUE_KEY[i]; face.key == VALUE_KEY
def test_failures(self):
    # FontError for: BBX != (w, h, 0, -descent); DWIDTH != w; a 12x24 value glyph inking column 0; a
    # 16x32 value glyph inking column 15; a 16x32 '.' inking column 11 (bold column 12 leaves [4, 12));
    # an 8x16 ':' inking column 1 (outside [2, 6), no bold on 8x16); 6x12 without 'A'; 8x16 without U+0178;
    # a 16x32 value face without '5' (one of the 15 key glyphs)
def test_release_checks(self):
    # synthetic tarball (tarfile, temp dir) whose four BDFs come from make_bdf (valid, so build_faces
    # succeeds): wrong tarball sha256 -> FontError; a member's wrong sha256 -> FontError.
    # main() reaches the LICENSE check only with the hashes patched: mock.patch.object(mod,
    # "TARBALL_SHA256", <synthetic>) and mock.patch.object(mod, "BDF_SHA256", <synthetic>), then
    # main(["--tarball", str(p), "--cache", str(tmp), "--out", str(tmp / "x.cpp")]):
    #   LICENSE != NOTICE -> 1, "LICENSE" in stderr, x.cpp not written;
    #   LICENSE == NOTICE (positive control) -> 0, x.cpp written
def test_cpp_output(self):
    # line 1 "// SPDX-License-Identifier: BSD-2-Clause"; the header's comment block, each line stripped of
    # "// " or a bare "//", contains NOTICE.splitlines() in order (blank lines as bare "//", D3);
    # clang-format off/on; D3's four descriptor lines; ASCII, LF, final newline, no trailing blank on any
    # line; deterministic
def test_micropython_contract(self):
    # render_micropython(faces, "abc123") imported from a temp dir: VERSION, VALUE_KEY, EXTRA; len(FOLD)
    # 33, FOLD[0] "°", FOLD[32] "Y"; with kFolds parsed from SteamiFont.cpp by this test:
    # [ord(c) for c in EXTRA[1:32]] == kFolds' Latin-1 codes in kFolds' order, FOLD[1:32] == their base
    # letters; F6X12 (6, 12, 9, None), F8X16 (8, 16, 12, None), F12X24 (12, 24, 19, VALUE_KEY), F16X32
    # (16, 32, 26, VALUE_KEY) as (w, h, ascent, key); type(data) is bytes, len 1536, 2048, 720, 960;
    # every glyph equals the bytes parsed from render_cpp(faces)'s text (the 0x.. of each kSpleen array),
    # never Face.data; the header holds VERSION, the tarball and four BDF sha256 values, the generator
    # path, "abc123", "BSD-2-Clause" and every NOTICE line
def test_committed_tables_match_the_release(self):
    # skipped unless ROOT/.spleen-cache/spleen-2.1.0.tar.gz and the committed .cpp exist (the .cpp lands
    # two commits later; Step 7's CI job is the unconditional check): render_cpp(...) == the file, byte
    # for byte
```

  Run: `make test-tooling`. Expected: FAIL (the script is missing).
- [ ] **Step 2: Implement** the script in `check-footprint.py`'s conventions: shebang; GPL SPDX on line 2; a Usage docstring; `from __future__ import annotations`; `argparse`; `sys.exit(main())`.
  - Each design 5.6 failure raises `FontError` naming the face and the code point.
  - The narrow check covers 8x16, 12x24 and 16x32. 6x12 is LABEL and never narrow, and w/4 is not an integer for it.
  - The bold checks cover 12x24 and 16x32.
- [ ] **Step 3: Run the tests.** Expected: PASS; `test_committed_tables_match_the_release` skipped (the pre-existing skips, such as `test_ssim_matches_scikit_image` without scikit-image, unchanged).
- [ ] **Step 4: Generate the tables.**
  - Add D2 to `SteamiFont.h`: no `constexpr` table in the header.
  - Add the `.gitignore` line, copy the release tarball into `.spleen-cache/`, and run `python3 scripts/gen-spleen-fonts.py` twice. Expected: exit 0 and a byte-identical file; `test_committed_tables_match_the_release` now runs and passes.
  - Run with `--micropython "$SCRATCH/spleen.py"` and import the result with `python3 -I`. Expected: no error. That file is never committed.
- [ ] **Step 5: Lint.** Run `make lint` (which tidies `SteamiFontSpleen.cpp` too) and `make build`. Expected: clean, `SUCCESS`. Any finding is fixed in the generator's template.
- [ ] **Step 6: Notices.**
  - In `scripts/arduino-package/README.md`, "GPL-3.0-or-later." becomes "GPL-3.0-or-later, with third-party fonts:". It is followed by:
    - the 8x8 font (MicroPython petme128 at b0a1b60a9b425cdb24e36d71974e902338ffb059, MIT) and its notice;
    - Spleen 2.1.0 (BSD-2-Clause) and `NOTICE`.
    - Each notice is verbatim, with the same line breaks, in a fenced `text` block, with no `{` or `}`.
  - `lib/steami_board/README.md` gets the same section.
  - New `test_package_readme_carries_the_font_notices` in `test_build_arduino_package.py`:
    - it renders the real `scripts/arduino-package/README.md` with `.format(version="1.2.3")`;
    - it asserts that `NOTICE`, the MIT block of `SteamiFont.cpp` lines 7-29 and `"v1.2.3"` are in the result. Each MIT line maps to `line[3:]` if it starts with `"// "`, else `""` (the bare `//` lines are blank lines of the notice);
    - the fixture README stays.
  - Run `make test-tooling`. Expected: OK.
- [ ] **Step 7: CI regeneration.** Add the job `spleen-fonts` to `lint.yml`:
  - checkout;
  - `actions/setup-python@v5` (`3.x`);
  - `actions/cache@v4` on `.spleen-cache`, key `spleen-2.1.0-8b47c56f1a6eb858fbcf9e34530557404b02fbb3455e38e64fb84473fd0c372f`;
  - `python3 scripts/gen-spleen-fonts.py --micropython "$RUNNER_TEMP/spleen.py"`;
  - `git diff --exit-code -- lib/steami_board/src/SteamiFontSpleen.cpp`.
- [ ] **Step 8: Verify.**
  - Run `make test-native` and the 128 px check.
  - Run `make package`, then `ls build/arduino-package/src/SteamiFontSpleen.cpp` and `grep -c "Frederic Cambus" build/arduino-package/README.md`.
  - Expected: PASS, 14 `OK`, the file present, a count ≥ 1.
- [ ] **Step 9: Commit** (the commit check first; the notices land before the data)

```bash
git add scripts/gen-spleen-fonts.py scripts/tests/test_gen_spleen_fonts.py .gitignore
git commit -m "feat(tooling): Generate the Spleen 2.1.0 tables and spleen.py."
git add scripts/arduino-package/README.md scripts/tests/test_build_arduino_package.py
git commit -m "docs(tooling): Ship the 8x8 and Spleen notices in the package README."
git add lib/steami_board/README.md
git commit -m "docs(board): Add the third-party fonts section to the README."
git add lib/steami_board/src/SteamiFont.h lib/steami_board/src/SteamiFontSpleen.cpp
git commit -m "feat(board): Add the generated Spleen 2.1.0 font tables."
git add .github/workflows/lint.yml
git commit -m "ci(ci): Check that the Spleen tables match their generator."
```

---

### Task 3: Extended glyph codes and face lookups

**Files:** Modify `lib/steami_board/src/SteamiFont.h`, `SteamiFont.cpp`, `tests/native/test_steami_display/test_main.cpp`.

**Interfaces:**
- Consumes: D2's descriptors; `kFolds`, `foldCodePoint`, `steamiNextGlyph` (`SteamiFont.cpp:243-335`).
- Produces (D5):
  - `uint8_t steamiNextCode(const char*& s);`
    - returns 32..159; '?' for any other code point, control byte or invalid sequence; 0 at the end or for nullptr;
    - moves s exactly as `steamiNextGlyph` does.
  - `uint8_t steamiFoldCode(uint8_t code);`: 128..158 → `kFolds`' base letter, 159 → 'Y', any other code unchanged.
  - `int8_t steamiValueIndex(uint8_t code);`: the position in a file-local `kValueKey[] = " +-.:0123456789"`, else −1.
  - `const uint8_t* steamiFaceGlyph(const SteamiFontFace& face, uint8_t code);`
    - full set: the glyph of the code, '?' outside 32..159;
    - value set: the glyph at `steamiValueIndex`, nullptr for −1;
    - the offset is computed in `size_t`.
  - `steamiNextGlyph(s)` == `steamiFoldCode(steamiNextCode(s))`. The 8x8/5x7 lookups keep '?' above 127.

- [ ] **Step 1: Write the failing tests**

```cpp
static void test_extended_codes(void) {
    // "A" 'A'; "\xC2\xB0" 127; "\xC3\x80" 128; "\xC3\xA9" 148; "\xC3\xBF" 158; "\xC5\xB8" 159; "\x7F",
    // "\x01", "\xE2\x82\xAC", "\xFF" '?'; "A\xC3" 'A', '?', 0; nullptr and "" 0, s unmoved; every step
    // moves s as steamiNextGlyph does
}
static void test_fold_codes(void) {
    // 128 'A', 148 'e', 158 'y', 159 'Y', 127 127, 'A' 'A'; for every string of the three UTF-8 tests:
    // steamiNextGlyph == steamiFoldCode(steamiNextCode) at each step
}
static void test_spleen_face_metrics(void) {
    // D2's {w, h, ascent, glyphs}; sum of glyphs * h * ((w + 7) / 8) == 5264
}
static void test_face_glyph_lookup(void) {
    // 8x16 ' ' 16 zero bytes; 8x16 'A' inks rows 2..11 only; codes 200 and 20 give '?'; 8x16 159 != 'Y',
    // 6x12 159 == 'Y'; 16x32 '5' == data + 10 * 32 * 2, 16x32 'A' nullptr; steamiValueIndex ' ' 0,
    // '.' 3, ':' 4, '9' 14, 'A' -1, 127 -1
}
```

  Run: `.venv/bin/pio test -e native -f native/test_steami_display`. Expected: build error.
- [ ] **Step 2: Implement.**
  - Split `steamiNextGlyph` before its `return foldCodePoint(cp)` into a shared decoder plus a code-point-to-code mapping.
  - `test_out_of_range_codes_give_question_mark` passes unchanged.
  - The `kFont8x8`/`kFont5x7` headers and the `kFolds` lines stay byte-stable (`golden_layout.py:31-35`).
- [ ] **Step 3: Verify.** Run that suite, `.venv/bin/pio test -e native_asan`, `make tidy`, `make build` and the 128 px check. Expected: PASS, 14 `OK`.
- [ ] **Step 4: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiFont.h lib/steami_board/src/SteamiFont.cpp tests/native/test_steami_display/test_main.cpp
git commit -m "feat(board): Decode the extended glyph codes of the Spleen faces."
```

---

### Task 4: Primitives on 240, `textHeight()` and `Drawing`

**Files:**
- Modify:
  - `lib/steami_board/src/SteamiDisplay.h/.cpp`;
  - `SteamiScreen.h/.cpp` (`glyphsAt` and its comment only);
  - `lib/steami_board/examples/Drawing/Drawing.ino`, `tests/native/helpers/SteamiGallery.h`.
- Test:
  - `tests/native/test_steami_display/test_main.cpp`;
  - `tests/native/test_steami_screen/test_main.cpp` and `golden_layout.py` (letters, the NE reference);
  - `tests/native/test_steami_screen_mockups/test_main.cpp` (07 and 09 on 240).

**Interfaces:**
- Consumes: `steamiNextCode`, `steamiFaceGlyph`, `STEAMI_SPLEEN_8X16/6X12`; `ready()`, `span()`, `width()`, `drawText(…, bool small)`.
- Produces:
  - public `int16_t textHeight(uint8_t scale = 1);` (non-const, touches no table): `16 · max(scale, 1)` if `spleenPanel()`, else `8 · max(scale, 1)`;
  - protected `bool spleenPanel();` (D1);
  - protected `void faceGlyph(const SteamiFontFace& face, const uint8_t* bits, uint8_t firstColumn, uint8_t columnCount, int32_t x, int32_t y, SteamiRgb c, uint8_t n, bool bold);` (D6; n 0 counts as 1; nullptr bits draw nothing);
  - private `void drawFaceText(const char* s, int16_t x, int16_t y, SteamiRgb c, uint8_t scale, const SteamiFontFace& face);`;
  - `glyphsAt` = `SteamiDisplay::text(s, polarX(d, deg) − textWidth(s) / 2, polarY(d, deg) − textHeight() / 2, c, 1)`, cast after the subtraction;
  - golden: `spleen()` (the parsed faces) and `r_text(cv, W, s, x, y, scale, c)` (8x8 × scale on 128, 8x16 × scale with accents on 240).

- [ ] **Step 1: Golden.**
  - Add a Spleen section that asserts the sizes 1536/2048/720/960 and D2's metrics.
  - `r_text` and `number_cell` move to the panel's face: `(px − 8·n/2, py − (8 or 16)/2)`.
  - Assert that 4b's 128 watch and compass numbers are unchanged.
  - Print the 240 WHITE boxes of the watch numbers and the N letter.
  - Run the golden script. Expected: exit 0.
- [ ] **Step 2: Write the failing tests.** `TestDisplay` gains `using SteamiDisplay::faceGlyph; using SteamiDisplay::spleenPanel;`.

```cpp
// glyphSizeCheck on 240: 'A' at (10, 10), scale s: x 10..10+7s-1, y 10+2s..10+12s-1, 44*s*s lit,
// pixelCalls() == 0 (128 as today)
// accentsCheck on 240: é != e, É != E, Ÿ != Y with Ÿ's box from row 10; degree x 11..15, y 11..14, 14 lit
// (128 as today: é == e, É == E, Ÿ == Y, degree 11..14 x 10..13)
// textClippingCheck on 240: "Hello" at h - 3 inks row h-1 only (font row 2); the 255 / +-32768 calls stay
// smallTextCheck on 240: 'A' x 10..14, y 11..18; widths unchanged
// invalidUtf8Check, scaleZeroCheck, test_text_width_counts_glyphs: unchanged on both panels
static void test_text_height(void) {   // Review Focus 3, 4
    // textHeight(s) for s 0..4: 8*max(s,1) on 128 and 96x96; 16*max(s,1) on 240 and 200x200;
    // textHeight(255) == 4080 on 240; no backend: 8, and 24 at scale 3; a 240 panel not begun: 16
}
static void test_text_follows_a_new_backend(void) {   // Review Focus 5
    // text("Ab", 10, 10) on 240; board.end(); inject 128: textHeight() == 8 and text() == a fresh 128 render
}
static void test_face_glyph_runs_and_bold(void) {
    // faceGlyph(8X16, '.', 2, 4, 10, 10, kInk, 1, false): lit == popcount of '.' columns 2..5, inside
    // x 10..13; n 2: 4x, pixelCalls() == 0; bold 16x32 '5', n 1: lit == sum of popcount(row | row >> 1)
    // (w-bit rows, MSB left), nothing at x >= 26; nullptr bits: nothing
}
static void test_text_on_240_is_8x16_and_never_bold(void) {
    // text("88", 10, 10, kInk, 2): lit == 2 * 4 * popcount(8x16 '8') and == text("8", 10, 10, kInk, 2)
    // plus text("8", 26, 10, kInk, 2) drawn together
}
static void test_gallery_text_on_240(void) {
    // drawGallery, 240 colour: WHITE box rows 62..71 inside x 96..143; YELLOW within rows 80..100:
    // box rows 85..95 inside x 96..143
}
```

  Screen suite:
  - the 240 pins of the watch numbers and of N take the prints;
  - new `test_letters_are_text_at_scale_1`: on 128 and 240, compass(0)'s WHITE pixels equal `text("N", polarX(L, 0) − textWidth("N") / 2, polarY(L, 0) − textHeight() / 2, STEAMI_WHITE, 1)` alone, with `L = radius() − u(STEAMI_COMPASS_INSET) + u(STEAMI_COMPASS_LETTERS)`;
  - the NE case (`test_main.cpp:1482-1500`) keeps its widget call but takes the reference `s.drawGlyphs(Run{"TE", 2, 1, Ink::LABEL, 2}, 110, STEAMI_GRAY)`. `textSmall(…, 2)` is now 6x12 × 2, while LABEL stays 5x7 until Task 5.

  Mockups 07 and 09 take the 240 prints.

  Run: `.venv/bin/pio test -e native -f native/test_steami_display`. Expected: build error (`textHeight`).
- [ ] **Step 3: Implement** D1, D6, `textHeight` and `glyphsAt`. Change `Drawing.ino` and `SteamiGallery.h` identically:
  - the two `text()` lines of design section 6;
  - the comment on `s`: "s scales the gauge strokes; text() follows the panel's font by itself."
- [ ] **Step 4: Verify.**
  - Run `make test-native`, `.venv/bin/pio test -e native_asan`, the golden script, `make test-tooling` (the Drawing/gallery rule passes unchanged), `make lint`, `make build` and `make test-example/steami_board/Drawing`. Then the 128 px check. Expected: PASS, 14 `OK`.
  - **240 gallery diff** (one-off, never committed):
    - render the baseline's gallery: `mkdir -p "$SCRATCH/gallery-before" && STEAMI_PPM_DIR="$SCRATCH/gallery-before" .venv/bin/pio test -d "$SCRATCH/base" -e native -f native/test_steami_display`, and this tree's the same way into `$SCRATCH/gallery-after` (the writers assert on a missing directory);
    - compare the two `steami-display-gallery-240-color.ppm` files in a scratch script;
    - expected: every differing pixel lies in x 72..167, rows 60..75 or 84..99;
    - keep both files for the PR.
  - **Linking:**
    - build: `PLATFORMIO_BUILD_DIR="$SCRATCH/draw" PLATFORMIO_SRC_DIR=lib/steami_board/examples/Drawing .venv/bin/pio run -e steami`;
    - inspect: `~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm -C -S --size-sort "$SCRATCH/draw/steami/firmware.elf" | grep -E 'kSpleen|STEAMI_SPLEEN_'` (data symbols only: a plain `grep -i spleen` also matches functions such as `SteamiDisplay::spleenPanel()`);
    - expected: exactly `(anonymous namespace)::kSpleen8x16` of size `00000800` and `STEAMI_SPLEEN_8X16`, with no 6X12, 12X24 or 16X32 symbol.
  - **Stack:** record the primitive text frames and `glyphsAt`. Expected: `compass` and `watch` ≤ 64.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiDisplay.* lib/steami_board/src/SteamiScreen.* lib/steami_board/examples/Drawing/ \
  tests/native/helpers/SteamiGallery.h tests/native/test_steami_display/ tests/native/test_steami_screen/ \
  tests/native/test_steami_screen_mockups/
git commit -m "feat(board): Draw text() in Spleen 8x16 on the 240 px screen." \
  -m "On the V1c, text() is now twice as tall at the same width (8x16 glyphs);
textHeight() gives the line pitch on either screen."
```

---

### Task 5: Kernel faces and metrics (TEXT and LABEL on the V1c)

**Files:**
- Modify:
  - `lib/steami_board/src/SteamiScreen.h/.cpp`;
  - `SteamiScreenConst.h` (`STEAMI_CELL_HEIGHT = 8`; the comment of `STEAMI_TITLE_CAP_CELLS` says "TEXT-face cells").
- Test: `tests/native/test_steami_screen/test_main.cpp`, `golden_layout.py`, `tests/native/test_steami_screen_mockups/test_main.cpp`.

**Interfaces:**
- Consumes: `spleenPanel()`, `faceGlyph()`, `steamiNextCode`, `steamiFaceGlyph`, `STEAMI_SPLEEN_*`; the 4b kernel (`SteamiScreen.h:100-272`).
- Produces (D7), all protected unless marked:
  - `enum class FaceId`;
  - `struct Run { const char* s; size_t glyphs; int32_t y; Ink ink; uint8_t f; uint8_t shrink = 0; FaceId face = FaceId::NONE; };`;
  - `FaceId faceFor(const char* s, Ink ink, uint8_t f);` (VALUE is F8X8 until Task 6);
  - `static uint8_t replication(FaceId face, uint8_t f);`, `static int32_t cellHeight(FaceId face, uint8_t f);`, `int32_t glyphAdvance(Ink ink, uint8_t f);` (`glyphAdvance(Ink::VALUE, f)` stays 8f on every panel until Task 6, D12);
  - `static int32_t advanceWidth(const char* s, size_t glyphs, Ink ink, int32_t advance);` (narrow VALUE glyphs count half);
  - `int32_t cellWidth(const Run& t, size_t glyphs);` = `advanceWidth(t.s, glyphs, t.ink, glyphAdvance(t.ink, t.f))`. It replaces the static 4-argument form, so every leftover old call fails to compile;
  - `int32_t valueWidth(const char* s, uint8_t f);`, no longer static;
  - `static uint32_t inkRows(const char* s, size_t glyphs, Ink ink, FaceId face);` (bit r = font row r);
  - private `Region rowsChord(uint32_t rows, const Run& t);` (last bit searched from 31): it takes y from `t.y` and computes n = `replication(t.face, t.f)` itself, so `fitGlyphs` (64 B in 4b) keeps the live set it has today across its loop;
  - private `static const SteamiFontFace* spleenFace(FaceId id);` (nullptr for NONE, F8X8 and F5X7);
  - `inkChord`, `fitGlyphs` and `drawGlyphs` read `t.face`; `inkChord` and `fitGlyphs` pass `t` to `rowsChord`, `drawGlyphs` uses n = `replication(t.face, t.f)`;
  - `drawGlyphs` keeps `glyph()` for F8X8/F5X7 and calls `faceGlyph` for Spleen faces;
  - `maxChars()` = `width() / glyphAdvance(Ink::TEXT, k())`.

- [ ] **Step 1: Golden.**
  - Model faces: `face_for`, `replication`, `advance`, `ink_rows(s, n, ink, face)`, `cell_width`, `draw_glyphs` on Spleen bitmaps. The 128 path stays 8x8/5x7; values stay 8x8 (D12); the label row stays `7*k` and the cap 13·8·k until Task 7.
  - Assert, from design section 6:
    - "Distance" at row 37, x 88..151, ink rows 39..48;
    - "VL53L1X ToF" at 202 with no rise, x 76..163, ink 204..213, chord [45, 195];
    - "0" at (52, 169) and "500" at (176, 169), ink 170..177;
    - the (−1e8, 1e8) cells `[(25, 84), (158, 211)]`;
    - the "100" min label shares no pixel with the DARK track;
    - "Temperature" 88 px at 37; the 01 and 08 subtitles at 202; 03's blocks at 177/197;
    - "Mood" 32 px at 37; `maxChars` 16 / 30; `text("ABCDEFGH", N, c, 2)` whole on 240.
  - Rewrite `golden_layout.py:1500-1515` for 240. The 128 asserts stay, under the comment "on 128".
  - Print every changed pin, and the "100" distance to the ring for the PR (design: 8.1 px).
- [ ] **Step 2: Write the failing tests.**
  - `TestScreen` gains `using` for `FaceId`, `faceFor`, `replication`, `cellHeight`, `glyphAdvance` and `advanceWidth`.
  - Each test `Run` sets `face` through `s.faceFor(…)`: `fit()` at `:351`, and the `drawGlyphs(Run{…})` and `inkChord(Run{…})` calls.
  - `static_assert(sizeof(Run) == 2 * sizeof(void*) + 2 * sizeof(int32_t));`.
  - The suite's expectation helpers become face-aware. They compile unchanged today but read only the 8x8/5x7 tables, so on 240 they would give wrong counts for every LABEL (6x12) and TEXT (8x16) part:
    - `runInk(s, glyphs, ink, f, FaceId face)` reads `steamiFaceGlyph` with replication n for a Spleen face (TEXT and LABEL; VALUE stays 8x8 until Task 6) and the 8x8/5x7 tables otherwise. It decodes with `steamiNextCode` for a Spleen face and keeps `steamiNextGlyph` (folded) for F8X8/F5X7, so 128 is unchanged and an accented string counts the ink of its accented glyph on 240. `valueInk(s, f, FaceId face)` (new third argument) and a new `wholeInk(s, f, face)` call it. Callers pass `s.faceFor(…)` (for `valueInk`, `s.faceFor(text, Ink::VALUE, f)`, F8X8 until Task 6);
    - `firstInkRow(s, face)` uses the 4-argument `inkRows` and searches up to the face's cell height (16 for F8X16; today's bound `row < 8`); its callers (`:941-942`) multiply the row by `s.replication(face, k)`, not by k (equal to k on 128, so 128 is unchanged);
    - the static `TestScreen::cellWidth(…)` calls at `:693` (test_title_band) and `:887` (subtitles) become `s.cellWidth(Run{…, s.faceFor(…)}, n)`;
    - call sites to switch: `:674`, `:859`, `:913`, `:941-942`, `:1138-1141`, `:1194`, `:1214`, `:1232`, `:1253`, `:1272`, `:1315`, `:1333`, `:1340`, `:1406`, `:1472`, `:1490`, `:1697-1699`;
    - on 128 every helper returns exactly what it returns in 4b, so no 128 assertion changes (the 128 literal check).

```cpp
static void test_face_for_text_and_label(void) {
    // 128 and 96: TEXT F8X8 (f 1..4), LABEL F5X7; 240 and 200: TEXT f 2, 4, 6 F8X16, f 3 NONE, LABEL
    // F6X12; replication (F8X8,3) 3, (F5X7,2) 2, (F6X12,2) 1, (F8X16,6) 3; cellHeight (F8X8,2) 16,
    // (F5X7,1) 7, (F6X12,2) 12, (F8X16,4) 32; glyphAdvance 128 (TEXT,1) 8, (LABEL,1) 6; 240 (TEXT,2) 8,
    // (TEXT,4) 16, (LABEL,2) 6, and (VALUE,2) 16 during the interim (D12; Task 6 changes it to 8)
}
// test_ink_rows_and_widths keeps its 128 values: inkRows("_", 1, TEXT, F8X8) 0xC0, ("TEMP", 4, LABEL,
// F5X7) 0x7F; advanceWidth("23.5", 4, VALUE, 8) 28, ("TEMP", 4, LABEL, 12) 48, ("Temp\xC3\xA9rature",
// 11, TEXT, 8) 88; s.valueWidth("23.5", 1) 28 and ("12:34:56", 1) 56 on 128; adds the printed 240 rows
// (inkRows("TEMP", 4, LABEL, F6X12) rows 1..8, inkRows("Hg", 2, TEXT, F8X16) rows 2..14)
// test_scale_quantities: maxChars 30 on 240
static void test_one_font_per_panel(void) {   // Review Focus 4
    // 128, 240 and 200: "\xC3\x89t\xC3\xA9" at n 1..3 and "\xC3\x89t\xC3\xA9 20\xC2\xB0C" at n 1:
    // text(s, CENTER, c, n) == SteamiDisplay::text(s, x, y, c, n) with tw = textWidth(s, n),
    // f = n * k, x = resolveX(CENTER, tw, f), y = resolveY(CENTER, tw, f)
}
static void test_label_equals_text_small(void) {
    // gauge(0, 0, 100): GRAY pixels left of centerX() == textSmall("0", 52, 169, GRAY, 1) (240) and
    // textSmall("0", 27, 90, GRAY, 1) (128)
}
static void test_accents_in_widgets(void) {
    // 240: title("\xC3\x89t\xC3\xA9") != title("Ete") and == text("Été", x, 37, GRAY) at its cell; 128:
    // == title("Ete"); 240: value(8, nullptr, CENTER, "\xC5\xB8") GRAY == value(8, nullptr, CENTER, "Y") GRAY
}
static void test_unknown_characters_in_widgets(void) {   // Review Focus 2
    // every panel: title("\x7F\xE2\x82\xAC") == title("??"); value(8, "mm", CENTER, "\x01T") ==
    // value(8, "mm", CENTER, "?T"); subtitle("A\xC3") == subtitle("A?")
}
static void test_faces_accept_empty_strings(void) {   // Review Focus 1
    // forEachPanel: title(nullptr), title(""), subtitle(""), text(nullptr, N), value(8, nullptr, NE,
    // nullptr), value(8, "", W, ""), gauge(0, 0, 0): no crash; no GRAY/LIGHT pixel for an empty part
}
```

  Also:
  - the accented title case (`:742-749`) compares on 128 only, and its 240 box (`:738-741`) takes the print;
  - `kGaugeLabels` and `kGaugeExtremeLabels` take the 240 prints, with the comment "on 128 the two overlap";
  - the NE reference becomes `textSmall(<kept>, <x>, <y>, STEAMI_GRAY, 1)`;
  - every other changed 240 pin of both suites takes its print.

  Run: `.venv/bin/pio test -e native -f native/test_steami_screen`. Expected: build error (`FaceId`).
- [ ] **Step 3: Implement** the TEXT and LABEL part of D7 and `STEAMI_CELL_HEIGHT`, with D12's interim for VALUE:
  - `glyphAdvance(Ink::VALUE, f)` is 8f on every panel ('.' and ':' 4f);
  - `placeValue` sets `value.face = faceFor(value.s, Ink::VALUE, value.f)` on each f tried (F8X8 here), and both parts of `drawValueParts` set theirs (D7), so no run is NONE and the 14 hashes hold.

  The header comments that name 8x8 or 5x7 (`SteamiScreen.h` L27, 45, 113-114, 140, 190-192, 226, 251-252, 259-260) now name faces.
- [ ] **Step 4: Verify.**
  - Run `make test-native`, `.venv/bin/pio test -e native_asan`, the golden script, `make lint`, `make build`, then the 128 px check. Expected: PASS, 14 `OK`.
  - Stack: every bound frame ≤ 64. A frame over 64 gets `__attribute__((noinline))` on `faceFor`, `glyphAdvance` or `cellWidth`, then smaller locals; failing that, stop and report.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/ tests/native/test_steami_screen_mockups/
git commit -m "feat(board): Draw widget text and labels in Spleen on the V1c." -m "maxChars() is 30 on the V1c."
```

---

### Task 6: Values on the V1c: ladder, fit floor, narrow '.' and ':', bold

**Files:**
- Modify: `lib/steami_board/src/SteamiScreen.h/.cpp`.
- Test: the two screen suites, `golden_layout.py`.

**Interfaces:**
- Consumes: Task 5's `FaceId`, `Run::face`, `faceFor`, `replication`, `glyphAdvance`, `cellWidth`, `inkRows`, `drawGlyphs`; `steamiValueIndex`.
- Produces:
  - `faceFor(s, Ink::VALUE, f)` follows D7.
  - `glyphAdvance(Ink::VALUE, f)` becomes 4f on a Spleen panel ('.' and ':' 2f), together with the ladder: the face's cell width (design 5.2). It stays 8f on 128. `valueWidth("12:30 PM", 2) == 60` below depends on it.
  - `placeValue` keeps Task 5's `value.face = faceFor(value.s, Ink::VALUE, value.f)` as its first statement and adds an early return: on NONE it returns false at once, leaving `value.y` untouched. Otherwise it runs as today, with `if (fits || value.f == k()) return true;`.
  - `value()`'s `while (!placeValue(t, at, unit)) t.f--;` is unchanged. f = k always has a face.
  - `drawGlyphs` on a Spleen face:
    - VALUE '.'/':' use the window `[w/4, w/4 + w/2)` and advance `(w/2)·n`;
    - bold applies to VALUE on F12X24 or F16X32.

- [ ] **Step 1: Golden.**
  - Model the ladder, the floor k and bold.
  - Assert, from design section 6:
    - "342" in 16x32 bold: 48 px at x 96, ink columns 98..142, rows 97..116;
    - 04's unit "mm" ink rows 128..134 and 01's "°C" 124..134 (the unit top is `value.y + 8f`, settled here);
    - "23.5" 56 px, ink 20 px tall;
    - "72" at top 69;
    - 03 at f 4, x 32 and x 152;
    - NE at f 2, row 37, region [36, 204], x 197;
    - "12:30 PM" at f 2 is 60 px;
    - the ink columns of '.' and of ':' (design 5.2): 3..4 in 8x16, 5..6 in 12x24, 7..8 in 16x32.
  - On the grid scale 1..8 × the nine anchors × {"8", "23.5", "-888.8", "12:30 PM", "Été"}, assert for every case on 240 that the chosen f is ≥ k and that `face_for(text, VALUE, chosen_f)` is not NONE (this excludes 5, 7, 11, 13 and every odd f of a text with letters). Print the chosen f and the WHITE boxes the tests pin.
- [ ] **Step 2: Write the failing tests**

```cpp
static void test_face_for_values(void) {
    // 240 "23.5", f 1..12: NONE, F8X16, F12X24, F16X32, NONE, F12X24, NONE, F16X32, F12X24, F8X16, NONE,
    // F16X32; "12:30 PM" f 2 F8X16, f 3 NONE, f 4 F8X16; nullptr f 4 F16X32; 128: F8X8 for every f;
    // glyphAdvance(VALUE, 2) 8 and (VALUE, 4) 16 on 240, (VALUE, 2) 16 on 128
}
static void test_value_with_letters_keeps_the_narrow_window(void) {
    // 240: valueWidth("12:30 PM", 2) == 60 (7 x 8 + 4); value("12:30 PM", nullptr, CENTER, nullptr,
    // WHITE, 1): not bold; ':' ink inside its 4 px window (printed columns)
}
static void test_prefix_measured_in_the_whole_face(void) {
    // 240: Run t{"23.5 kPa", 8, 20, VALUE, 4, 0, faceFor("23.5 kPa", VALUE, 4)} (F8X16: "kPa" is no value
    // text); fitGlyphs(t, &room), room {centerX() - 30, centerX() + 29}: keeps 4 ("23.5", 56 px) and
    // returns the chord of 8x16 x 2's rows, not 16x32's (both printed); value() never cuts above f = k
}
static void test_value_fit_floor_and_skipped_factors(void) {
    // the printed grid cases (WHITE boxes); on 240 a CENTER, a W/E and a corner value that fit nowhere
    // end at f 2, cut there; value(8888, nullptr, W, nullptr, WHITE, 4) ends at the printed f (f 8 too
    // wide, f 7 faceless)
}
static void test_bold_stays_inside_the_cells(void) {
    // 240 at f 4 (16x32) and scale 3 (f 6, 12x24 x 2): no WHITE pixel outside [x0, x0 + valueWidth),
    // x0 printed; WHITE count == the golden's bold count
}
static void test_point_centred_in_its_window(void) {
    // 240, "8.8" at f 2, 3, 4, 6, 8: the '.' cell's left gap to its ink == the gap from its ink to the
    // cell's right edge, plus n when bold
}
static void test_unknown_characters_in_values(void) {   // Review Focus 2
    // 240: value("\xE2\x82\xAC" "5") == value("?5"), drawn by the TEXT ladder (8x16, not bold)
}
static void test_values_at_scale_255(void) {   // Review Focus 3
    // forEachPanel: value(8888, "mm", W, "LABEL", WHITE, 255), value("12:30", nullptr, NE, nullptr,
    // WHITE, 255), text("AB", CENTER, WHITE, 255): return, nothing outside the circle
}
```

  Also:
  - In `test_face_for_text_and_label` (Task 5), drop the interim pin `glyphAdvance(VALUE, 2) == 16` on 240: it moves to `test_face_for_values` as 8.
  - `runInk`/`valueInk(s, f, face)` (Task 5, signature unchanged here) gain the VALUE-face branch: `steamiFaceGlyph` of the value face with its replication, the narrow window `[w/4, w/4 + w/2)` for '.' and ':', and the bold rule `row | row >> 1` (w-bit rows, MSB left, as in Task 4's `faceGlyph` test) on F12X24 and F16X32. Re-check the value call sites `:1138-1141`, `:1194`, `:1214`, `:1232`, `:1253`, `:1272`, `:1406`, `:1490`, `:1697`; on 128 they return what they return in 4b.
  - The NE case checks the value at f 2, from x 197 on row 37, with the count taken from the 8x16 '8'. Its GRAY label reference is a call, not a value pin: it becomes `textSmall(<kept>, <x>, 16, STEAMI_GRAY, 1)` with the printed kept text and x (cell top 37 − 7 − 14 = 16 under the 7k label rule, which Task 7 replaces).
  - Every 240 value pin (01, 02, 03, 04, `kHiBoxes` and the value arrays) takes its print, including 04's "mm" and 01's "°C" rows asserted in Step 1.

  Run: `.venv/bin/pio test -e native -f native/test_steami_screen`. Expected: FAIL.
- [ ] **Step 3: Implement** the five items above (the VALUE ladder and the VALUE advance switch in the same commit). The comments of `value()`, `placeValue()` and `fitValue()` change to "down to k" and "at f = k".
- [ ] **Step 4: Verify** as in Task 5. `value` stays at 64; `placeValue` and `fitValue` ≤ 64.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/ tests/native/test_steami_screen_mockups/
git commit -m "feat(board): Draw V1c values in bold Spleen 12x24 and 16x32."
```

---

### Task 7: Layout rules: title cap and band, label height

**Files:**
- Modify: `lib/steami_board/src/SteamiScreen.cpp` (`marginNs`, `drawValueParts`).
- Test: the two screen suites, `golden_layout.py`.

**Interfaces:**
- Consumes: `glyphAdvance`, `cellHeight`, `faceFor`, `STEAMI_CELL_HEIGHT`.
- Produces:
  - `marginNs`: `half13 = STEAMI_TITLE_CAP_CELLS * glyphAdvance(Ink::TEXT, k()) / 2` (52 on both panels). The cap becomes 29 / 14; the floor stays at 37, so every 240 title sits at row 37.
  - `drawValueParts`: the label's cell top is `value.y − u(STEAMI_VALUE_LABEL_GAP) − cellHeight(part.face, part.f)` (7k / 12), with `part.face` set first.
  - `SteamiScreen.cpp` names neither `CHAR_W` nor `CHAR_H`.

- [ ] **Step 1: Golden.** Model both rules, then assert:
  - `cap() == (29, 14)`;
  - "ABCDEFGHIJKLMNOPQRSTUV" (22 cells) whole at row 37 on 240, and a 23-cell title cut to 22 (rows 39..51: 177 px);
  - NE label: cell top 18, ink 19..26, room [58, 182], "TEMP" whole at x 159;
  - 03's labels at cell top 72; 04's "mm" and 01's "°C" rows (Task 6) unchanged.

  Print the changed pins.
- [ ] **Step 2: Write the failing tests.**
  - On 240, `marginNs(176, 2)` and `marginNs(208, 2)` are 37.
  - `kTitles` and the cut cases take the prints.
  - New `test_titles_stay_on_row_37`: on 240, every `kTitles` entry and the 22-cell title have cell top 37 and ink from row 39 at most; the 23-cell title keeps 22 glyphs.
  - The NE label reference (Task 6's cell top 16) becomes `textSmall("TEMP", 159, 18, STEAMI_GRAY, 1)`, with ink rows 19..26.
  - Mockup 03 takes the prints (its labels move; 04 has no label and a short title, so no pixel of 04 moves here).

  Run: `.venv/bin/pio test -e native -f native/test_steami_screen`. Expected: FAIL.
- [ ] **Step 3: Implement** both rules.
- [ ] **Step 4: Verify** as in Task 5. `drawValueParts` ≤ 64.
  - `grep -v '^\s*//' lib/steami_board/src/SteamiScreen.cpp | grep -c 'CHAR_W\|CHAR_H'` gives 0.
  - This is a check, not a test: `test_steami_display_rules.py` stays unchanged.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiScreen.cpp tests/native/test_steami_screen/ tests/native/test_steami_screen_mockups/
git commit -m "feat(board): Keep V1c titles at row 37 and size labels by face."
```

---

### Task 8: Program 04 clear of the ring, long strings, review scores

**Files:**
- Test: the two screen suites, `golden_layout.py`.

**Interfaces:**
- Consumes: the widgets of Tasks 4 to 7; the mockups suite's own `maskAlone(size, color, c, draw)`, `maskOf(mem, c)` and `any(mask)` (it has no `rendered()` and no `colourMask`; neither exists in that suite); the golden's `gauge_render`, `colour_set`, `text_layout`, `subtitle_layout`.
- Produces: the PR's SSIM table and distance figures (scratch).

- [ ] **Step 1: Golden** (`Program 04 on 240 (4b-font Task 8)`).
  - Assert that these renders, each drawn alone, share no pixel with the DARK track of `gauge(0, 0, 500)`:
    - the lit pixels of `title("Distance")`;
    - the lit pixels of `subtitle("VL53L1X ToF")`;
    - the GRAY labels of `gauge(342, 0, 500)`.
  - Print each one's minimum pixel-centre distance to that track. The design gives 13.6, 30.5, 12.7 and 9.4; report a gap above 0.5 px, which is not a gate.
  - Reprint the mockup boxes; they must equal the pins.
- [ ] **Step 2: Write the tests**

```cpp
static void test_mockup_04_text_clears_the_ring(void) {   // mockups suite, 240 colour
    // track = maskAlone(240, true, DARK, gauge(0, 0, 500)); no pixel of maskAlone(240, true, GRAY,
    // title("Distance")), of maskAlone(240, true, DARK, subtitle("VL53L1X ToF")) (each drawn alone) or of
    // maskAlone(240, true, GRAY, gauge(342, 0, 500)) is set in track
}
static void test_long_strings_stay_inside(void) {   // screen suite, forEachPanel
    // 40-glyph strings mixing accents, digits, '.', ':' and letters: title, 3-line subtitle, text and
    // value (label and unit) at every anchor and scale 1..4, gauge labels (+-1e8): outsideCircle() == 0
}
```

  Run: `.venv/bin/pio test -e native -f native/test_steami_screen_mockups -f native/test_steami_screen`. Expected: PASS. A failure goes back to the owning task.
- [ ] **Step 3: Review scores.**
  - Render the baseline's mockups: `mkdir -p "$SCRATCH/ppm-before" "$SCRATCH/ppm-after" && STEAMI_PPM_DIR="$SCRATCH/ppm-before" .venv/bin/pio test -d "$SCRATCH/base" -e native -f native/test_steami_screen_mockups`. Render this tree's into `$SCRATCH/ppm-after`.
  - Score each with `python3 scripts/mockup-review.py --ppm-dir <dir> --refs /home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups --out <dir>/review`.
  - Expected: every 240 SSIM rises and every 128 row is identical. Keep both pages and the table. A 240 drop is reported, not a gate.
- [ ] **Step 4: Verify.** Run `make test-native`, `.venv/bin/pio test -e native_asan`, the golden script, `make lint` and the 128 px check.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add tests/native/test_steami_screen/ tests/native/test_steami_screen_mockups/
git commit -m "test(tests): Check that program 04 draws no text on the V1c ring."
```

---

### Task 9: The spec edits of design section 9

**Files:** Modify `docs/superpowers/specs/2026-11-25-steami-display-design.md`.

**Interfaces:**
- Consumes: the "Spec:" list of design section 9. Its line numbers are those of fc9a7e9, still exact; match each item by its quoted text. Also the golden script's prints after Task 8, and D10.
- Produces: the spec as the single authority.

- [ ] **Step 1:** Apply every item in order.
  - Every 240 figure (5.9 value row, size table, section 7 cases, known differences) comes from the golden output.
  - Decision 7 lists "F1 to F18". The design's "F1 to F17" predates its F18, the MicroPython review. This is a deliberate deviation from the text of design section 9; Task 12 Step 6 lists it in the PR text.
  - The program 04 entry follows D10.
  - The 4b-font row of section 8 is design section 10's row, placed between 4b and 4c.
- [ ] **Step 2: Check** with `grep -n "only font the student sees\|big-digits\|5 visible rows on both\|cut to TE at x 110\|down to 1"` on the spec. Expected: no hit, except text that says the statement was revisited.
- [ ] **Step 3: Commit** (the commit check first)

```bash
git add docs/superpowers/specs/2026-11-25-steami-display-design.md
git commit -m "docs(docs): Amend the display spec for the V1c fonts."
```

---

### Task 10: README and keywords

**Files:** Modify `lib/steami_board/README.md` (L430-431, L508, L534-536) and `lib/steami_board/keywords.txt`.

**Interfaces:**
- Consumes: `textHeight(uint8_t scale = 1)`, `maxChars()` 30, and the "Third-party fonts" section of Task 2.
- Produces: the README list of design section 9.

- [ ] **Step 1: Write.**
  - The `text()` row and a new `int16_t textHeight(uint8_t scale = 1)` row, both in design section 9's wording.
  - `maxChars()`: "16 on 128, 30 on 240".
  - L534-536 in the design's wording.
  - One sentence under the primitives table: since this release, `text()` is 16 px tall at scale 1 on the V1c, and stacked lines use `textHeight()`.
  - Nothing to remove: the README holds no 240 gauge-label difference.
  - `keywords.txt`: `textHeight<TAB>KEYWORD2` after `textWidth` (L138).
- [ ] **Step 2: Check.** Run `make package`, then `grep -n "15 on 240\|twice as large" lib/steami_board/README.md`. Expected: no hit.
- [ ] **Step 3: Commit** (the commit check first)

```bash
git add lib/steami_board/README.md lib/steami_board/keywords.txt
git commit -m "docs(board): Document textHeight() and the V1c fonts."
```

---

### Task 11: Footprint gate and full verification

**Files:** Modify `scripts/check-footprint.py` (`SKETCHES`) and `scripts/tests/test_check_footprint.py`.

**Interfaces:**
- Consumes: all earlier tasks and the baseline tree.
- Produces: the PR figures.

- [ ] **Step 1:**
  - New `test_sketches`: `SKETCHES == ("steami_board/Drawing", "steami_board/ScreenGauge")`. Run `make test-tooling`. Expected: FAIL.
  - Set the tuple. Expected: OK.
  - Run `.venv/bin/python3 scripts/check-footprint.py`. Expected: exit 0, two lines, both within the limits.
- [ ] **Step 2: Commit** (the commit check first)

```bash
git add scripts/check-footprint.py scripts/tests/test_check_footprint.py
git commit -m "feat(tooling): Add ScreenGauge to the footprint gate."
```

- [ ] **Step 3: PR figures** (scratch only).
  - **Flash and RAM of Drawing and ScreenGauge**, each build in its own directory:
    - before: `PLATFORMIO_BUILD_DIR="$SCRATCH/fp-base-<Name>" PLATFORMIO_SRC_DIR="$SCRATCH/base/lib/steami_board/examples/<Name>" .venv/bin/pio run -d "$SCRATCH/base" -e steami`;
    - after: the same build from this tree into `$SCRATCH/fp-<Name>`.
    - Expected: about +6 KB for ScreenGauge and +3 KB for Drawing. A Drawing delta near 4.5 KB means `text()` reaches the 6x12 blob: back to Task 4.
  - **`nm` of Drawing**, as in Task 4. Expected: the 8x16 blob only.
  - **The full stack table**, next to 4b's. Expected: every bound frame ≤ 64.
- [ ] **Step 4: Full verification.** All must succeed:
  - `make test-native`.
  - `.venv/bin/pio test -e native_asan` with `STEAMI_PPM_DIR`, then Task 1's `sha256sum -c`: no `runtime error:`, no sanitizer report, 14 `OK`.
  - The golden script.
  - `make lint`, `make build`, `make test-examples`, `make test-tooling`, `make package`.
  - `python3 scripts/gen-spleen-fonts.py --tarball <release>`, then `git diff --exit-code -- lib/steami_board/src/SteamiFontSpleen.cpp`: no diff.
  - Commits:
    - `npx commitlint --from origin/main --to HEAD` passes;
    - in `git log --format=%s fc9a7e9..`: headers ≤ 72 characters, scopes `board|tests|tooling|ci|docs`, no `!`, no `[skip ci]`, no attribution lines;
    - `git log --format=%b fc9a7e9.. | grep -c BREAKING` gives 0.
  - Optional, with network access: arduino-cli on 3.1.0 in a scratch data directory. Otherwise the `arduino-package` CI job is the check.

---

### Task 12: Hardware acceptance on the V1c

**Controller-run on hardware, needs a board slot from the wiki-steami-71 session; subagents never flash or open the serial port.**

**Files:** none committed. Sketches, binaries and photos stay in the controller's scratchpad.

**Interfaces:**
- Consumes:
  - ScreenTemperature, ScreenBattery, ScreenComfort, ScreenGauge, ScreenCompass, ScreenFaces, ScreenWatch and Drawing;
  - the 240 renders of Task 8 and the galleries of Task 4;
  - the `_ref.png` files;
  - the webcam `/dev/video4`.
- Produces: the photos and findings for the PR.

- [ ] **Step 1: Build and flash.**
  - Before the first flash, ask the slot owner (wiki-steami-71) which binary is on the board and record its path; Step 5 restores that exact file.
  - Build with `PLATFORMIO_BUILD_DIR="$SCRATCH/hw-<Name>" PLATFORMIO_SRC_DIR=lib/steami_board/examples/<Name> .venv/bin/pio run -e steami`, then `arm-none-eabi-objcopy -O binary`.
  - Flash by DAPLink copy with `/tmp/claude-1000/-home-nedjar-sandbox-wiki-steami/1346e80f-1de2-4e4c-bae2-38b41b348575/scratchpad/tools/flash.sh <bin> <seconds>`. Never `pio run -t upload`, OpenOCD or `scripts/capture-serial.py`.
  - Space the flashes out. On "Bad response 8 -> 17" or a stalled drive, stop: recovery needs the user's consent, and wiki-steami-71 does it.
- [ ] **Step 2: Photos.**
  - Run `uvcdynctrl -d /dev/video4 -s "Auto Exposure" 3`.
  - Then run `ffmpeg -hide_banner -loglevel error -f v4l2 -input_format mjpeg -i /dev/video4 -vf "select=gte(n\,60),scale=1280:-1,format=yuvj420p" -frames:v 1 -y <file>.jpg`. The 60 warm-up frames matter.
  - Rotate −90° before comparing.
- [ ] **Step 3: Drawing** matches the new 240 gallery dump:
  - the shapes are unchanged;
  - "STeaMi" 48 px at x 96, ink rows 62..71;
  - "20°C é" 48 px at x 96, ink rows 85..95, with the accent drawn;
  - the median of five `show(): <n> us` lines stays unchanged, within 57 600..75 000 µs.
- [ ] **Step 4: Examples 01 to 04 and 07 to 09**, against their 240 renders and `_ref.png` (design section 6):
  - 01: title at 37, the value 16x32 bold, "BME280" at 202;
  - 02: the value at top 69, the block at 177/197;
  - 03: side values at f 4, labels in 6x12;
  - **04, blocking:**
    - no text on the ring;
    - "Distance" at x 88..151;
    - "VL53L1X ToF" at row 202, between the ring ends;
    - "0" and "500" clear of the ring;
    - compare the layout, not the live number;
  - 07: N E S W in 8x16, upright;
  - 08: "Mood" at 37, subtitles at 202;
  - 09: 12 3 6 9 in 8x16, hands at about 10:10:(30 + t).

  A V1c mismatch outside the spec's known differences blocks the PR and goes back to its task (decision 6, B5). Program 04's 128 overlap is not checked on hardware.
- [ ] **Step 5: Restore.** Reflash the binary recorded in Step 1. Ask the user to put the board back if it was moved. Release the slot.
- [ ] **Step 6: PR text.**
  - "Closes #280"; the PR is stacked on #277, a draft until it is rebased onto `main` after #277 merges.
  - The photos next to `_ref.png` and the 240 renders. Drawing next to the before and after gallery PNGs, with the "only the two text cells differ" result.
  - The SSIM table.
  - Flash and RAM, before (fc9a7e9) and after, with the `nm` result.
  - The stack table next to 4b's. The 14 unchanged 128 hashes.
  - The tested package notices. Drawing's `show()` time. The paths outside row 10 ("Paths").
  - The removed 240 gauge-label differences (8.1 px), the 128 ones that stay, and the program 04 overlap on 128 only (D10).
  - V1a/V1b not checked on hardware (decision 6). "Both cores" read as PlatformIO plus arduino-cli on STM32duino 3.1.0.
  - The visible change (D11), with the request that the squash commit body keep D11's sentences.
  - The deliberate deviation from design 9: decision 7 lists F1 to F18, not F1 to F17 (Task 9).
  - The two `CONTRIBUTING.md` exceptions ("Paths").
  - The controller files the MicroPython issue (design 8.1) when the PR opens, and the wiki issue (8.2) after the release.

---

## Out of scope

- **4c:** `graph`, `menu` (the six-row menu with its scanline-clipped bar, and graph labels placed by ink edges, which this delivery only writes into the spec), `SteamiHistory`, examples 05 and 06, WeatherStation and its `SKETCHES` entry.
- **MicroPython (design 8.1).** `spleen.py` is generated here only into temporary directories, for its tests and CI, and is committed nowhere in this repository. The `device.py` changes, blits, palette and key, the framebuf stub, the `FLASH_FS` move and timing all belong to the MicroPython PR.
- **The wiki** (design 8.2).
- **4b's own pending items:** the compass's known-north check and the maintainer's decision on example 04 (PR #277).
- **The design's non-goals:**
  - Spleen on 128;
  - 5x8 or 32x64 faces;
  - full ASCII in 12x24 and 16x32;
  - value faces, bold or the narrow window in primitives;
  - anti-aliasing;
  - the drivers' own `text()`.
- **Untouched files:** a Makefile target (D4), `platformio.ini`, `requirements.txt`, `lib/ssd1327`, `lib/gc9a01`, `scripts/mockup-review.py`, `scripts/build-arduino-package.py`.
