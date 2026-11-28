# Changelog

All notable changes to this project will be documented in this file.
See [Conventional Commits](https://conventionalcommits.org) for commit guidelines.

## [0.46.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.46.0...v0.46.1) (2026-11-28)


### Bug Fixes

* Make setTemperatureOffset() replace the two-point calibration. ([#324](https://github.com/steamicc/arduino-steami-lib/issues/324)) ([e63e332](https://github.com/steamicc/arduino-steami-lib/commit/e63e332c8ce22967b6081d6a2c3e303b7fe7ca6c)), closes [#323](https://github.com/steamicc/arduino-steami-lib/issues/323) [#79](https://github.com/steamicc/arduino-steami-lib/issues/79)

# [0.46.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.45.0...v0.46.0) (2026-11-28)


### Features

* **board:** Add named constants for the expander pads. ([#319](https://github.com/steamicc/arduino-steami-lib/issues/319)) ([e22a789](https://github.com/steamicc/arduino-steami-lib/commit/e22a789837770539aac4e087a00ae38258f72686)), closes [steamicc/micropython-steami-lib#494](https://github.com/steamicc/micropython-steami-lib/issues/494) [#318](https://github.com/steamicc/arduino-steami-lib/issues/318)
* **board:** Catch button taps made between two calls. ([#322](https://github.com/steamicc/arduino-steami-lib/issues/322)) ([1ec9f13](https://github.com/steamicc/arduino-steami-lib/commit/1ec9f1363d78fca4ac265b40fbe0ddaf95b0c4e8)), closes [#251](https://github.com/steamicc/arduino-steami-lib/issues/251)
* Expose the driver methods that only MicroPython offers. ([#317](https://github.com/steamicc/arduino-steami-lib/issues/317)) ([bd37dfa](https://github.com/steamicc/arduino-steami-lib/commit/bd37dfa7ec325be5a633a664ee7534e1c52e4f61)), closes [#316](https://github.com/steamicc/arduino-steami-lib/issues/316)

# [0.45.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.44.0...v0.45.0) (2026-11-28)


### Features

* **bq27441:** Add battery_logger and charging_monitor examples. ([#313](https://github.com/steamicc/arduino-steami-lib/issues/313)) ([6bd9572](https://github.com/steamicc/arduino-steami-lib/commit/6bd9572b061316be2bd7e6d3c5a71ceabfa1d3ca))
* **ism330dl:** Add the TiltMaze and DigitalLevel screen examples. ([#315](https://github.com/steamicc/arduino-steami-lib/issues/315)) ([6eb9d0b](https://github.com/steamicc/arduino-steami-lib/commit/6eb9d0b587da15928206a34a0530aaeac4eff370)), closes [#75](https://github.com/steamicc/arduino-steami-lib/issues/75) [#312](https://github.com/steamicc/arduino-steami-lib/issues/312)

# [0.44.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.43.0...v0.44.0) (2026-11-28)


### Features

* **daplink_flash:** Add info, read-back and gated erase examples. ([#311](https://github.com/steamicc/arduino-steami-lib/issues/311)) ([804da9f](https://github.com/steamicc/arduino-steami-lib/commit/804da9f16461eaf536ee41f7b201269289c70755)), closes [#78](https://github.com/steamicc/arduino-steami-lib/issues/78)

# [0.43.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.42.4...v0.43.0) (2026-11-28)


### Features

* **im34dt05:** Add the SoundLogger and VoiceActivityDetector examples. ([#308](https://github.com/steamicc/arduino-steami-lib/issues/308)) ([9c28f68](https://github.com/steamicc/arduino-steami-lib/commit/9c28f68c16942a5f4b26084cc6615cbdc9ec0d7a)), closes [#82](https://github.com/steamicc/arduino-steami-lib/issues/82)

## [0.42.4](https://github.com/steamicc/arduino-steami-lib/compare/v0.42.3...v0.42.4) (2026-11-28)


### Bug Fixes

* **mcp23009e:** Report every interrupt edge in order, with its level. ([#314](https://github.com/steamicc/arduino-steami-lib/issues/314)) ([ba81db4](https://github.com/steamicc/arduino-steami-lib/commit/ba81db40b36f49dd1f5062c1a8f2b6ff1061ef0f)), closes [steamicc/micropython-steami-lib#466](https://github.com/steamicc/micropython-steami-lib/issues/466) [#183](https://github.com/steamicc/arduino-steami-lib/issues/183) [#303](https://github.com/steamicc/arduino-steami-lib/issues/303)

## [0.42.3](https://github.com/steamicc/arduino-steami-lib/compare/v0.42.2...v0.42.3) (2026-11-28)


### Bug Fixes

* **lis2mdl:** Read the field in one burst and report bus errors. ([#310](https://github.com/steamicc/arduino-steami-lib/issues/310)) ([90bbbd5](https://github.com/steamicc/arduino-steami-lib/commit/90bbbd58e22b120821352542a7583fb8aee24f21)), closes [#309](https://github.com/steamicc/arduino-steami-lib/issues/309) [#309](https://github.com/steamicc/arduino-steami-lib/issues/309)

## [0.42.2](https://github.com/steamicc/arduino-steami-lib/compare/v0.42.1...v0.42.2) (2026-11-28)


### Bug Fixes

* **board:** Fall back to the legacy hts calibration for WSEN-HIDS. ([#301](https://github.com/steamicc/arduino-steami-lib/issues/301)) ([e8c725a](https://github.com/steamicc/arduino-steami-lib/commit/e8c725af6a213569e95d6851ffab0786b5f51fd5)), closes [#300](https://github.com/steamicc/arduino-steami-lib/issues/300)

## [0.42.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.42.0...v0.42.1) (2026-11-28)


### Bug Fixes

* **bme280:** Avoid left shifts of negative values in the compensation. ([#298](https://github.com/steamicc/arduino-steami-lib/issues/298)) ([93038de](https://github.com/steamicc/arduino-steami-lib/commit/93038ded586d6060e9484fab37cc47af421599e9)), closes [#259](https://github.com/steamicc/arduino-steami-lib/issues/259)

# [0.42.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.41.0...v0.42.0) (2026-11-28)


### Features

* **lis2mdl:** Add setTemperatureOffset() and keep setTempOffset() alias. ([#296](https://github.com/steamicc/arduino-steami-lib/issues/296)) ([f682d59](https://github.com/steamicc/arduino-steami-lib/commit/f682d59998e3b071eb7ce662396f10dcdd5f9583))

# [0.41.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.40.0...v0.41.0) (2026-11-27)


### Features

* **board:** Add the scrolling graph, the menu and SteamiHistory. ([#289](https://github.com/steamicc/arduino-steami-lib/issues/289)) ([cc05443](https://github.com/steamicc/arduino-steami-lib/commit/cc054437356205b5d8014cdaacd4587bf0536ecb))

# [0.40.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.39.1...v0.40.0) (2026-11-27)


### Features

* **board:** Draw round widgets, and V1c text in Spleen fonts. ([#277](https://github.com/steamicc/arduino-steami-lib/issues/277)) ([64b8db6](https://github.com/steamicc/arduino-steami-lib/commit/64b8db6b513de902ac30c72f7d134915bc8688bd)), closes [#287](https://github.com/steamicc/arduino-steami-lib/issues/287)

## [0.39.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.39.0...v0.39.1) (2026-11-27)


### Bug Fixes

* **board:** Wait for a sample at the new accelerometer range. ([#279](https://github.com/steamicc/arduino-steami-lib/issues/279)) ([946a24c](https://github.com/steamicc/arduino-steami-lib/commit/946a24c937d5754f95cfdc0aa19b3be770bf317b)), closes [#278](https://github.com/steamicc/arduino-steami-lib/issues/278)

# [0.39.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.38.0...v0.39.0) (2026-11-26)


### Features

* **im34dt05:** Add the IMP34DT05 microphone driver. ([#276](https://github.com/steamicc/arduino-steami-lib/issues/276)) ([e1a11a0](https://github.com/steamicc/arduino-steami-lib/commit/e1a11a0ed06edda78dae2cb196ea63c3fcdd2da8)), closes [#16](https://github.com/steamicc/arduino-steami-lib/issues/16) [#82](https://github.com/steamicc/arduino-steami-lib/issues/82) [hi#pass](https://github.com/hi/issues/pass) [hi#pass](https://github.com/hi/issues/pass) [hi#pass](https://github.com/hi/issues/pass)

# [0.38.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.37.0...v0.38.0) (2026-11-26)


### Features

* **board:** Lay out titles, values and bars on the screen. ([#274](https://github.com/steamicc/arduino-steami-lib/issues/274)) ([b960407](https://github.com/steamicc/arduino-steami-lib/commit/b960407cae8fc218d0d5b3b26cacbe74ce0a6031))

# [0.37.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.36.3...v0.37.0) (2026-11-26)


### Features

* **board:** Draw on the SSD1327 and GC9A01 screens. ([#270](https://github.com/steamicc/arduino-steami-lib/issues/270)) ([50d789a](https://github.com/steamicc/arduino-steami-lib/commit/50d789ab77ba2f5e6e51739743afaefe58a812aa)), closes [#266](https://github.com/steamicc/arduino-steami-lib/issues/266)

## [0.36.3](https://github.com/steamicc/arduino-steami-lib/compare/v0.36.2...v0.36.3) (2026-11-26)


### Bug Fixes

* **gc9a01,ssd1327:** Initialise the panel driver members in place. ([#267](https://github.com/steamicc/arduino-steami-lib/issues/267)) ([f22b255](https://github.com/steamicc/arduino-steami-lib/commit/f22b255206a3129aaa4553ee410df7c25f653dfe)), closes [#266](https://github.com/steamicc/arduino-steami-lib/issues/266)

## [0.36.2](https://github.com/steamicc/arduino-steami-lib/compare/v0.36.1...v0.36.2) (2026-11-26)


### Bug Fixes

* **ci:** Attach the examples archive to a draft release. ([#268](https://github.com/steamicc/arduino-steami-lib/issues/268)) ([5fdd119](https://github.com/steamicc/arduino-steami-lib/commit/5fdd119886510d136b6b3e3766bdbb7ff26ec3f3)), closes [#265](https://github.com/steamicc/arduino-steami-lib/issues/265)

## [0.36.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.36.0...v0.36.1) (2026-11-26)


### Bug Fixes

* **ssd1327:** Mark the writeCmd and writeData overrides. ([#264](https://github.com/steamicc/arduino-steami-lib/issues/264)) ([187ac10](https://github.com/steamicc/arduino-steami-lib/commit/187ac10b3ec125b39a4b27a0e8f5ea7fe8867381))

# [0.36.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.35.0...v0.36.0) (2026-11-26)


### Features

* **tooling:** Attach compiled examples to the distribution release. ([#262](https://github.com/steamicc/arduino-steami-lib/issues/262)) ([23d4a97](https://github.com/steamicc/arduino-steami-lib/commit/23d4a971573105294c30db718bdfd7989d4caf4f)), closes [#261](https://github.com/steamicc/arduino-steami-lib/issues/261)

# [0.35.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.34.0...v0.35.0) (2026-11-26)


### Features

* **board:** Add SteamiDisplay primitives on a memory backend. ([#258](https://github.com/steamicc/arduino-steami-lib/issues/258)) ([649221b](https://github.com/steamicc/arduino-steami-lib/commit/649221b40ca5814ab72725806e57fb43e9da2d1f))

# [0.34.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.33.0...v0.34.0) (2026-11-25)


### Features

* **board:** Add the motion, compass, light and button classes. ([#252](https://github.com/steamicc/arduino-steami-lib/issues/252)) ([0b12744](https://github.com/steamicc/arduino-steami-lib/commit/0b127442c8acd92762ea3ce0ac77f542cfeb6e18)), closes [#249](https://github.com/steamicc/arduino-steami-lib/issues/249)

# [0.33.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.32.0...v0.33.0) (2026-11-25)


### Features

* **vl53l1x:** Add practical usage examples. ([#250](https://github.com/steamicc/arduino-steami-lib/issues/250)) ([4cce660](https://github.com/steamicc/arduino-steami-lib/commit/4cce6605df8d9c1e67d7e22ca6cd8585c44a8989)), closes [#198](https://github.com/steamicc/arduino-steami-lib/issues/198) [#72](https://github.com/steamicc/arduino-steami-lib/issues/72) [#194](https://github.com/steamicc/arduino-steami-lib/issues/194)

# [0.32.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.31.1...v0.32.0) (2026-11-25)


### Features

* **board:** Add the first per-function classes. ([#248](https://github.com/steamicc/arduino-steami-lib/issues/248)) ([1cf0b0f](https://github.com/steamicc/arduino-steami-lib/commit/1cf0b0f1710c20505bb6e65f3cae67d461e9b9e8))

## [0.31.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.31.0...v0.31.1) (2026-11-25)


### Bug Fixes

* **board:** Claim every candidate chip in the BoardInfo example. ([#246](https://github.com/steamicc/arduino-steami-lib/issues/246)) ([25d95f7](https://github.com/steamicc/arduino-steami-lib/commit/25d95f783446c73d5acf1853a74f8a1bc15d027b)), closes [#245](https://github.com/steamicc/arduino-steami-lib/issues/245)

# [0.31.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.30.0...v0.31.0) (2026-11-25)


### Features

* **board:** Add SteamiBoard, board detection and shared drivers. ([#244](https://github.com/steamicc/arduino-steami-lib/issues/244)) ([c98082c](https://github.com/steamicc/arduino-steami-lib/commit/c98082c9dea99e4be24fc494a0423d7913b54f6a))

# [0.30.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.29.0...v0.30.0) (2026-11-24)


### Features

* Prefix the drivers with Steami to avoid Library Manager clashes ([#242](https://github.com/steamicc/arduino-steami-lib/issues/242)). ([008b93d](https://github.com/steamicc/arduino-steami-lib/commit/008b93db908c6c439504ee541016586ecb958709))

# [0.29.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.28.0...v0.29.0) (2026-11-24)


### Features

* **bme280:** Implement the Arduino driver for the V1c sensor ([#235](https://github.com/steamicc/arduino-steami-lib/issues/235)). ([b27c6dc](https://github.com/steamicc/arduino-steami-lib/commit/b27c6dcb0fabc153278aa2405be242bd3456febe))

# [0.28.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.27.0...v0.28.0) (2026-11-24)


### Features

* **daplink_flash:** Add the user file driver on the DAPLink bridge ([#167](https://github.com/steamicc/arduino-steami-lib/issues/167)). ([6513c3c](https://github.com/steamicc/arduino-steami-lib/commit/6513c3c3858eb75b2ba7bad33765c77cf960f67d)), closes [#12](https://github.com/steamicc/arduino-steami-lib/issues/12) [#32](https://github.com/steamicc/arduino-steami-lib/issues/32) [#177](https://github.com/steamicc/arduino-steami-lib/issues/177)

# [0.27.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.26.3...v0.27.0) (2026-11-24)


### Features

* **gc9a01:** Implement the Arduino driver for the V1c round TFT ([#234](https://github.com/steamicc/arduino-steami-lib/issues/234)). ([8d034e9](https://github.com/steamicc/arduino-steami-lib/commit/8d034e928249eb445fe1527e28b6328bc2e9fcc2)), closes [#15](https://github.com/steamicc/arduino-steami-lib/issues/15)

## [0.26.3](https://github.com/steamicc/arduino-steami-lib/compare/v0.26.2...v0.26.3) (2026-11-24)


### Bug Fixes

* **daplink_bridge:** Read command errors before the firmware clears them ([#239](https://github.com/steamicc/arduino-steami-lib/issues/239)). ([03adca6](https://github.com/steamicc/arduino-steami-lib/commit/03adca6b4fe01bdc143065a44b492ce1509989de)), closes [#238](https://github.com/steamicc/arduino-steami-lib/issues/238)

## [0.26.2](https://github.com/steamicc/arduino-steami-lib/compare/v0.26.1...v0.26.2) (2026-11-24)


### Bug Fixes

* **daplink_bridge:** Read the whole config zone, detect old firmwares ([#212](https://github.com/steamicc/arduino-steami-lib/issues/212)) ([337d138](https://github.com/steamicc/arduino-steami-lib/commit/337d138d1c53c84318740ce471b33724f965fac7))

## [0.26.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.26.0...v0.26.1) (2026-11-24)

# [0.26.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.25.1...v0.26.0) (2026-11-24)


### Features

* **bq27441:** Add battery_status and low_battery_alert examples. ([#188](https://github.com/steamicc/arduino-steami-lib/issues/188)) ([fa920df](https://github.com/steamicc/arduino-steami-lib/commit/fa920dfbaf6c07028205a80f38d0c1218304555c))

## [0.25.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.25.0...v0.25.1) (2026-11-24)


### Bug Fixes

* **ci:** Publish the Arduino package on published releases. ([#228](https://github.com/steamicc/arduino-steami-lib/issues/228)) ([484fd4b](https://github.com/steamicc/arduino-steami-lib/commit/484fd4bc9bbe6b87b86ddc4e4db5511ef5901d31)), closes [#227](https://github.com/steamicc/arduino-steami-lib/issues/227)

# [0.25.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.24.1...v0.25.0) (2026-11-24)


### Features

* **tooling:** Publish the drivers as one Arduino Library Manager package. ([#226](https://github.com/steamicc/arduino-steami-lib/issues/226)) ([9efdc10](https://github.com/steamicc/arduino-steami-lib/commit/9efdc101e7ec6678b829d0f5f47884ee0c398c58)), closes [#221](https://github.com/steamicc/arduino-steami-lib/issues/221)

## [0.24.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.24.0...v0.24.1) (2026-11-24)


### Bug Fixes

* **mcp23009e:** Rename example sketches and fix broken example links ([#218](https://github.com/steamicc/arduino-steami-lib/issues/218)). ([2ffc8ca](https://github.com/steamicc/arduino-steami-lib/commit/2ffc8caaa9b5c634fc13cd3fcc671a58b49730ff)), closes [#217](https://github.com/steamicc/arduino-steami-lib/issues/217) [#194](https://github.com/steamicc/arduino-steami-lib/issues/194) [#217](https://github.com/steamicc/arduino-steami-lib/issues/217)
* **tooling:** Really compile the examples in test-examples.py ([#223](https://github.com/steamicc/arduino-steami-lib/issues/223)). ([9f1bb72](https://github.com/steamicc/arduino-steami-lib/commit/9f1bb725a7e3d42ea0507e94ae419dae89b5280a)), closes [#222](https://github.com/steamicc/arduino-steami-lib/issues/222)

# [0.24.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.23.0...v0.24.0) (2026-09-16)


### Features

* **apds9960:** Implement Arduino driver ([#203](https://github.com/steamicc/arduino-steami-lib/issues/203)). ([9d3164f](https://github.com/steamicc/arduino-steami-lib/commit/9d3164f87eca854733ae9d2418705e7a0861b4ba)), closes [#204](https://github.com/steamicc/arduino-steami-lib/issues/204) [#205](https://github.com/steamicc/arduino-steami-lib/issues/205) [#210](https://github.com/steamicc/arduino-steami-lib/issues/210)

# [0.23.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.22.0...v0.23.0) (2026-09-15)


### Features

* **ism330dl:** Implement Arduino driver ([#199](https://github.com/steamicc/arduino-steami-lib/issues/199)). ([14c9136](https://github.com/steamicc/arduino-steami-lib/commit/14c9136cb0e301dc6918d631053f2ae5a9c833be)), closes [#201](https://github.com/steamicc/arduino-steami-lib/issues/201) [#202](https://github.com/steamicc/arduino-steami-lib/issues/202) [#209](https://github.com/steamicc/arduino-steami-lib/issues/209)

# [0.22.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.21.0...v0.22.0) (2026-09-14)


### Features

* **ssd1327:** Implement Arduino driver [#11](https://github.com/steamicc/arduino-steami-lib/issues/11) ([#192](https://github.com/steamicc/arduino-steami-lib/issues/192)). ([ed03c59](https://github.com/steamicc/arduino-steami-lib/commit/ed03c593cd1cffeee79ab41518fd97b9a63047af)), closes [#ifdef](https://github.com/steamicc/arduino-steami-lib/issues/ifdef) [#196](https://github.com/steamicc/arduino-steami-lib/issues/196)

# [0.21.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.20.0...v0.21.0) (2026-06-22)


### Features

* **vl53l1x:** Implement Arduino driver.  [#9](https://github.com/steamicc/arduino-steami-lib/issues/9) ([#191](https://github.com/steamicc/arduino-steami-lib/issues/191)) ([d78c4a3](https://github.com/steamicc/arduino-steami-lib/commit/d78c4a3480c6f9bce67e4848bdb29c4117005c7c))

# [0.20.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.19.0...v0.20.0) (2026-06-22)


### Features

* **lis2mdl:** Implement Arduino driver.  [#4](https://github.com/steamicc/arduino-steami-lib/issues/4) ([#190](https://github.com/steamicc/arduino-steami-lib/issues/190)) ([d89fc2a](https://github.com/steamicc/arduino-steami-lib/commit/d89fc2acaa1c6497403ba3433a447aefe5259780))

# [0.19.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.18.0...v0.19.0) (2026-05-27)


### Features

* **mcp23009e:** Add driver for the on-board GPIO expander. ([#172](https://github.com/steamicc/arduino-steami-lib/issues/172)) ([f3ca686](https://github.com/steamicc/arduino-steami-lib/commit/f3ca686afc36fd7e92124ddb22d9bc99cb465cb7))

# [0.18.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.17.0...v0.18.0) (2026-05-27)


### Features

* **bq27441:** Add driver for the on-board Li-Po fuel gauge. ([#171](https://github.com/steamicc/arduino-steami-lib/issues/171)) ([1d98747](https://github.com/steamicc/arduino-steami-lib/commit/1d98747eed10a959826b275226ccd85295d8cc46)), closes [hi#byte](https://github.com/hi/issues/byte) [#ifdef](https://github.com/steamicc/arduino-steami-lib/issues/ifdef)

# [0.17.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.16.0...v0.17.0) (2026-05-26)


### Features

* **wsen-hids:** Add examples. ([#160](https://github.com/steamicc/arduino-steami-lib/issues/160)) ([4b1759b](https://github.com/steamicc/arduino-steami-lib/commit/4b1759b87e5fa1e7ff65287c438549f94303ec62))

# [0.16.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.15.0...v0.16.0) (2026-05-26)


### Features

* **tooling:** Add YAML-driven qualification harness. ([#168](https://github.com/steamicc/arduino-steami-lib/issues/168)) ([b54f3af](https://github.com/steamicc/arduino-steami-lib/commit/b54f3afc8b564b103c0fc953336a4ea8321c9906)), closes [#124](https://github.com/steamicc/arduino-steami-lib/issues/124)

# [0.15.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.14.0...v0.15.0) (2026-05-26)


### Features

* **ble:** Add hot/cold treasure hunt example with BLE RSSI. ([#141](https://github.com/steamicc/arduino-steami-lib/issues/141)) ([0b9fe61](https://github.com/steamicc/arduino-steami-lib/commit/0b9fe61e8a4cd0477ca9cc5bad31bae5177b4c62))

# [0.14.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.13.0...v0.14.0) (2026-05-26)


### Features

* **ble:** Add BLE indoor map with real-time serial display. ([#150](https://github.com/steamicc/arduino-steami-lib/issues/150)) ([3e86103](https://github.com/steamicc/arduino-steami-lib/commit/3e861034c71cd7b7de024950567e1d9785917279))

# [0.13.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.12.0...v0.13.0) (2026-05-26)


### Features

* **ble:** Add BLE trilateration indoor positioning example. ([#147](https://github.com/steamicc/arduino-steami-lib/issues/147)) ([89c5cc1](https://github.com/steamicc/arduino-steami-lib/commit/89c5cc1b133cb4eb003be04c207ebab5e4a7beb6))

# [0.12.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.11.0...v0.12.0) (2026-05-26)


### Features

* **ble:** Add BLE RSSI room mapper example with CSV export. ([#142](https://github.com/steamicc/arduino-steami-lib/issues/142)) ([a966bec](https://github.com/steamicc/arduino-steami-lib/commit/a966bec490106f3204aaf69fa17efc9d9c6eed00))

# [0.11.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.10.0...v0.11.0) (2026-05-26)


### Features

* **ble:** Add RSSI proximity detector example. ([#140](https://github.com/steamicc/arduino-steami-lib/issues/140)) ([85124fb](https://github.com/steamicc/arduino-steami-lib/commit/85124fbe5eb30c8349e225f5fa198c1a640a0d84))

# [0.10.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.9.0...v0.10.0) (2026-05-26)


### Features

* **ble:** Add BLE beacon advertising example with WSEN-HIDS sensor. ([#139](https://github.com/steamicc/arduino-steami-lib/issues/139)) ([43ec925](https://github.com/steamicc/arduino-steami-lib/commit/43ec92559fe3c39020cb023f8a9fe08c17e5a219))

# [0.9.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.8.0...v0.9.0) (2026-05-26)


### Features

* **wsen-hids:** Add basic driver. ([#131](https://github.com/steamicc/arduino-steami-lib/issues/131)) ([e0cab75](https://github.com/steamicc/arduino-steami-lib/commit/e0cab752a3f58da090babc7d70bafa6a0aab70ea)), closes [#pragma](https://github.com/steamicc/arduino-steami-lib/issues/pragma) [steamicc/micropython-steami-lib#425](https://github.com/steamicc/micropython-steami-lib/issues/425) [hi#level](https://github.com/hi/issues/level) [steamicc/micropython-steami-lib#425](https://github.com/steamicc/micropython-steami-lib/issues/425) [steamicc/micropython-steami-lib#425](https://github.com/steamicc/micropython-steami-lib/issues/425)

# [0.8.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.7.4...v0.8.0) (2026-04-29)


### Features

* **daplink_bridge:** Implement Arduino driver. ([#161](https://github.com/steamicc/arduino-steami-lib/issues/161)) ([4b7ab0b](https://github.com/steamicc/arduino-steami-lib/commit/4b7ab0b85a6316e6079d0654c789bec9992fefe4)), closes [#156](https://github.com/steamicc/arduino-steami-lib/issues/156)

## [0.7.4](https://github.com/steamicc/arduino-steami-lib/compare/v0.7.3...v0.7.4) (2026-04-29)

## [0.7.3](https://github.com/steamicc/arduino-steami-lib/compare/v0.7.2...v0.7.3) (2026-04-29)

## [0.7.2](https://github.com/steamicc/arduino-steami-lib/compare/v0.7.1...v0.7.2) (2026-04-29)

## [0.7.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.7.0...v0.7.1) (2026-04-28)

# [0.7.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.6.0...v0.7.0) (2026-04-28)


### Features

* **wsen-pads:** Add three practical example sketches. ([#152](https://github.com/steamicc/arduino-steami-lib/issues/152)) ([f6bc295](https://github.com/steamicc/arduino-steami-lib/commit/f6bc2950d9d307a1a985ed3d637d68ef12d9beae)), closes [#69](https://github.com/steamicc/arduino-steami-lib/issues/69)

# [0.6.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.5.0...v0.6.0) (2026-04-28)


### Features

* **wsen-pads:** Implement Arduino driver. ([#143](https://github.com/steamicc/arduino-steami-lib/issues/143)) ([86a17d8](https://github.com/steamicc/arduino-steami-lib/commit/86a17d82f76ea5c4e8844cd5bf9cdb56e3bb1bc8)), closes [#5](https://github.com/steamicc/arduino-steami-lib/issues/5) [#25](https://github.com/steamicc/arduino-steami-lib/issues/25)

# [0.5.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.4.0...v0.5.0) (2026-04-28)


### Features

* **tooling:** Add capture-<driver>/<example> for boot serial capture. ([#138](https://github.com/steamicc/arduino-steami-lib/issues/138)) ([6e23015](https://github.com/steamicc/arduino-steami-lib/commit/6e23015cfbd46ffdcc8af1cb1fea561d02e570b6))

# [0.4.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.3.0...v0.4.0) (2026-04-28)


### Features

* **tooling:** Add per-example flash- targets and list-examples ([#133](https://github.com/steamicc/arduino-steami-lib/issues/133)) ([df21d26](https://github.com/steamicc/arduino-steami-lib/commit/df21d26ceb0241e1dd441231eba9f6b83643a6c3))

# [0.3.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.2.5...v0.3.0) (2026-04-23)


### Features

* Add ble scope. ([#146](https://github.com/steamicc/arduino-steami-lib/issues/146)) ([63b1734](https://github.com/steamicc/arduino-steami-lib/commit/63b17341dc9f82f396ba00ff4b9fc3777c1f3a58)), closes [#145](https://github.com/steamicc/arduino-steami-lib/issues/145) [#Changes](https://github.com/steamicc/arduino-steami-lib/issues/Changes)

## [0.2.5](https://github.com/steamicc/arduino-steami-lib/compare/v0.2.4...v0.2.5) (2026-04-22)

## [0.2.4](https://github.com/steamicc/arduino-steami-lib/compare/v0.2.3...v0.2.4) (2026-04-22)

## [0.2.3](https://github.com/steamicc/arduino-steami-lib/compare/v0.2.2...v0.2.3) (2026-04-22)

## [0.2.2](https://github.com/steamicc/arduino-steami-lib/compare/v0.2.1...v0.2.2) (2026-04-22)

## [0.2.1](https://github.com/steamicc/arduino-steami-lib/compare/v0.2.0...v0.2.1) (2026-04-22)

# [0.2.0](https://github.com/steamicc/arduino-steami-lib/compare/v0.1.0...v0.2.0) (2026-04-21)


### Bug Fixes

* **hts221:** Make temperature_alarm beep non-blocking for true 100/100 ms. ([8c670b0](https://github.com/steamicc/arduino-steami-lib/commit/8c670b0d4ce9bea7ced201154645574e800354e4))
* **hts221:** Return NaN from dewPoint() when humidity is zero. ([6b2efaa](https://github.com/steamicc/arduino-steami-lib/commit/6b2efaa369e8f32ff21a2a0643fc2e15bf618a94))
* **hts221:** Wait for USB CDC enumeration in example setup(). ([0a3942d](https://github.com/steamicc/arduino-steami-lib/commit/0a3942d9cbc1f0291a7713e19f3b26ba264aefb8))


### Features

* **hts221:** Add three practical example sketches. ([75d6205](https://github.com/steamicc/arduino-steami-lib/commit/75d620530430056de64201b659905bb467aa62a7))

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
