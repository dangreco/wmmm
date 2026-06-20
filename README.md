# wmmm

[![CI](https://github.com/dangreco/wmmm/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/dangreco/wmmm/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/dangreco/wmmm?sort=semver)](https://github.com/dangreco/wmmm/releases)

A hardware-agnostic C++23 library for parsing the **AUP/WMMM UART protocol** used
by the TA640FC-W-ULX thermostat, intended for use as an ESPHome custom component.

## Status

The AUP framing layer is implemented as an incremental, byte-at-a-time parser
(`wmmm::aup::Decoder`) that reassembles `wmmm::aup::Frame` frames from an
asynchronous UART stream, and a matching `wmmm::aup::Encoder` that serialises
frames back to their on-wire byte sequence. The two are exact inverses for any
well-formed frame. See `include/aup.hpp` for the documented public API.

## Building and testing

```sh
task build   # configure + compile (CMake + Ninja)
task test    # build and run the unit tests via CTest
task check   # clang-format / yamllint checks
```

## Releases

Releases follow [Semantic Versioning](https://semver.org) and are tagged on `main`.
See the [Releases page](https://github.com/dangreco/wmmm/releases) and
[CHANGELOG.md](CHANGELOG.md). To pin a release in ESPHome, see
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

Licensed under the **GNU General Public License v3.0 or later**
(`SPDX-License-Identifier: GPL-3.0-or-later`). See [LICENSE](LICENSE) for the full
text.

## Reverse-engineering and non-affiliation notice

The AUP/WMMM protocol was **independently reverse-engineered** through clean-room,
black-box observation of the device, solely for interoperability. This project
contains **no proprietary source code, firmware, or other copyrighted material**
from the device manufacturer, and is **not affiliated with, authorized by, or
endorsed by** the manufacturer of the TA640FC-W-ULX or any related entity. All
trademarks are the property of their respective owners. See [NOTICE](NOTICE) for
details.
