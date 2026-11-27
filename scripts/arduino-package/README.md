# STeaMi

Arduino library for the [STeaMi](https://wiki.steami.cc) board: drivers for its
sensors, D-PAD, battery gauge and display, plus BLE examples.

This repository is generated from
[`steamicc/arduino-steami-lib`](https://github.com/steamicc/arduino-steami-lib)
v{version}. Do not edit it here: issues and pull requests go to the upstream
repository.

## Installation

Install **STeaMi** from the Arduino IDE Library Manager (search "STeaMi").

In the Boards Manager, install the STM32 MCU based boards package, then select
**STeaMi Board** in *STM32 MCU based boards*.

## Usage

The sensors are on the internal I²C bus:

```cpp
TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
```

Examples are available in *File > Examples > STeaMi*.

## Documentation

See the wiki: <https://wiki.steami.cc>.

## License

GPL-3.0-or-later, with third-party fonts:

The 8x8 font of the 128 px screen is MicroPython's `extmod/font_petme128_8x8.h`
(commit b0a1b60a9b425cdb24e36d71974e902338ffb059), under the MIT license:

```text
This file is part of the MicroPython project, http://micropython.org/

The MIT License (MIT)

Copyright (c) 2013, 2014 Damien P. George

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

The fonts of the 240 px screen are generated from Spleen 2.1.0
(<https://github.com/fcambus/spleen>), under the BSD-2-Clause license:

```text
Copyright (c) 2018-2024, Frederic Cambus
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```
