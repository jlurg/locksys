# Third-party notices

LockSys is licensed under the Apache License, Version 2.0 (see [LICENSE](LICENSE)).
This file lists the third-party components that are vendored in this repository or
linked into the LockSys firmware and application images, together with the notices
their licenses require. Versions and checksums are pinned in
[`tools/versions.env`](tools/versions.env).

## Vendored source code

Vendored code lives under `third_party/` and is never modified in place.

| Component | Version | License | Used in | Upstream |
|---|---|---|---|---|
| CMSIS-Core (Cortex-M), `CMSIS/Core/Include` only | 5.9.0 | Apache-2.0 | DCU firmware (headers) | <https://github.com/ARM-software/CMSIS_5> |
| STM32F1 CMSIS device headers | v4.3.5 | Apache-2.0 | DCU firmware (headers) | <https://github.com/STMicroelectronics/cmsis-device-f1> |
| nanopb (runtime and generator) | 0.4.9.2 | Zlib | CGW firmware, code generation | <https://github.com/nanopb/nanopb> |
| QR Code generator library (C) | 1.8.0 | MIT | CGW firmware | <https://github.com/nayuki/QR-Code-generator> |

## Components linked from SDKs and toolchains

| Component | Version | License | Used in | Notes |
|---|---|---|---|---|
| ESP-IDF and its components (FreeRTOS kernel, lwIP, Mbed TLS, Wi-Fi libraries) | v5.5.5 | Apache-2.0; FreeRTOS kernel MIT; lwIP BSD-3-Clause; Mbed TLS Apache-2.0 | CGW firmware | The per-release SBOM lists every component and license. |
| espressif/led_strip | 3.0.3 | Apache-2.0 | CGW firmware | ESP Component Registry managed component, pinned in `firmware/cgw/dependencies.lock`. |
| IAR DLIB runtime library | EWARM 9.70 | IAR Systems license agreement | DCU firmware | Redistribution terms follow the IAR license agreement. |
| Flutter engine, framework and Dart packages | Flutter 3.47.6 | BSD-3-Clause and package licenses | APP | The app shows the complete list on its licenses page. |

## Development and test tools

The following tools are used for building and testing only and are not distributed
with LockSys images: Unity, CMock and Ceedling (MIT); the Python packages locked in
`uv.lock`, including python-can (LGPL-3.0, used unmodified as a dependency of the HIL
tooling and never vendored); and the linters and formatters pinned in
`.pre-commit-config.yaml`.

## License texts

### Apache License 2.0

CMSIS-Core: Copyright (c) Arm Limited. STM32F1 CMSIS device headers: Copyright (c)
STMicroelectronics. espressif/led_strip: Copyright (c) Espressif Systems (Shanghai) CO LTD.
All three are licensed under the Apache License, Version 2.0, whose full
text is in [LICENSE](LICENSE).

### nanopb (Zlib)

```text
Copyright (c) 2011 Petteri Aimonen <jpa at nanopb.mail.kapsi.fi>

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held liable
for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you
   must not claim that you wrote the original software. If you use
   this software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
   must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
   distribution.
```

### QR Code generator library (MIT)

```text
QR Code generator library (C)

Copyright (c) Project Nayuki. (MIT License)
https://www.nayuki.io/page/qr-code-generator-library

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
- The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
- The Software is provided "as is", without warranty of any kind, express or
  implied, including but not limited to the warranties of merchantability,
  fitness for a particular purpose and noninfringement. In no event shall the
  authors or copyright holders be liable for any claim, damages or other
  liability, whether in an action of contract, tort or otherwise, arising from,
  out of or in connection with the Software or the use or other dealings in the
  Software.
```
