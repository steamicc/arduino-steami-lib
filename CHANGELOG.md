# Changelog

All notable changes to this project will be documented in this file.
See [Conventional Commits](https://conventionalcommits.org) for commit guidelines.

# [0.1.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.0.6...v0.1.0) (2026-04-21)


### Bug Fixes

* **hts221:** Surface waitForDataReady timeouts and short I2C reads. ([b6d983f](https://github.com/steamicc/arduino-steami-lib/commit/b6d983fd0f03cee95f71c7df5b16d12e25618b01))
* **hts221:** Use the STeaMi internal I2C bus in the smoke-test and example. ([874421d](https://github.com/steamicc/arduino-steami-lib/commit/874421dbbc6a2534ebc5f57a8b8dcac252a21f0d))
* **tooling:** Pin mock millis() to uint32_t and correct the comment. ([727f45f](https://github.com/steamicc/arduino-steami-lib/commit/727f45fb5553c3836ba2901644a1f6a048267050))


### Features

* Exercise HTS221 in the smoke-test sketch. ([8294d53](https://github.com/steamicc/arduino-steami-lib/commit/8294d53b4ce254572693ccf3adab1f1262c6d31e)), closes [#102](https://github.com/steamicc/arduino-steami-lib/issues/102)
* **hts221:** Add driver implementing the STeaMi convention API. ([7516e54](https://github.com/steamicc/arduino-steami-lib/commit/7516e54a70bee385a87e1480ad619b6734c85c76))

## [0.0.6](https://github.com/steamicc/arduino-steami-lib/compare/v0.0.5...v0.0.6) (2026-04-21)

## [0.0.5](https://github.com/steamicc/arduino-steami-lib/compare/v0.0.4...v0.0.5) (2026-04-21)


### Bug Fixes

* Capture every byte of a multi-byte I2C write in the Wire mock. ([dd4dac7](https://github.com/steamicc/arduino-steami-lib/commit/dd4dac70c7bcf2b6feca79bd4f873804181774fb))
* Track current register pointer per I2C address in Wire mock. ([a6fab3d](https://github.com/steamicc/arduino-steami-lib/commit/a6fab3d241540abae01317eedf0906c2b97a11fd))

## [0.0.4](https://github.com/steamicc/arduino-steami-lib/compare/v0.0.3...v0.0.4) (2026-04-21)


### Bug Fixes

* Inline mock Wire instance to guarantee it links into native tests. ([393d22b](https://github.com/steamicc/arduino-steami-lib/commit/393d22b12d16352fb46b332ba37623fd54ebe837))
* Use test-native in make ci to keep it board-less. ([fb6444c](https://github.com/steamicc/arduino-steami-lib/commit/fb6444cc0099fa2c6b0ea8c2ae99378947232f25))

## [0.0.3](https://github.com/steamicc/arduino-steami-lib/compare/v0.0.2...v0.0.3) (2026-04-21)

## [0.0.2](https://github.com/steamicc/arduino-steami-lib/compare/v0.0.1...v0.0.2) (2026-04-21)


### Bug Fixes

* **board:** Align flash and RAM sizes with STM32duino STeaMi entry. ([13c0810](https://github.com/steamicc/arduino-steami-lib/commit/13c0810a92eef348a40fcc9bffa53d002ecae33c))
* Point PlatformIO at the repo's tests/ directory. ([37d70a0](https://github.com/steamicc/arduino-steami-lib/commit/37d70a06b3f1e4da520b909ba0f313fa50e236cf))
* Set CMSIS-DAP as the default and onboard debug tool. ([db72f21](https://github.com/steamicc/arduino-steami-lib/commit/db72f21881d9d0e5fbe2e0b0d94667386d03db89))
* Skip make test when no test/ directory exists. ([76a7d4c](https://github.com/steamicc/arduino-steami-lib/commit/76a7d4c59ab80d482f1d7dbcc59389a995bde2bc))
* Use STM32WB55_CM4.svd for CPU1 peripheral debug symbols. ([bc725f8](https://github.com/steamicc/arduino-steami-lib/commit/bc725f8669f3649bd49910079df5f5dbbc4813c1))
