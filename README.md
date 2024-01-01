<div align="center">
<img src="./docs/assets/icon.png" width="150" align="center">

# [materialgram](https://github.com/kukuruzka165/materialgram)
<a href='https://flathub.org/apps/io.github.kukuruzka165.materialgram'>
  <img width='175' alt='Download on Flathub' src='https://dl.flathub.org/assets/badges/flathub-badge-en.svg'/>
</a>

![Hits](https://img.shields.io/endpoint?url=https%3A%2F%2Fhits.dwyl.com%2Fkukuruzka165%2Fmaterialgram.json%3Fcolor%3Dlightgray)
[![Telegram Channel](https://img.shields.io/badge/channel-blue?logo=telegram&labelColor=gray)](https://t.me/materialgram)
[![Telegram Group](https://img.shields.io/badge/chat-blue?logo=telegram&labelColor=gray)](https://t.me/materialgram_chat)
[![AUR](https://img.shields.io/badge/AUR%20package-blue?logo=archlinux&labelColor=gray)](https://aur.archlinux.org/packages?K=materialgram)
[![GitHub Downloads](https://img.shields.io/github/downloads/kukuruzka165/materialgram/total?logo=github)](https://github.com/kukuruzka165/materialgram/releases/latest)

[**Telegram Desktop**](https://github.com/telegramdesktop/tdesktop) fork with Material Design and other improvements, which is based on the [Telegram API][telegram_api] and the [MTProto][telegram_proto] secure protocol.

The source code is published under GPLv3 with OpenSSL exception, the license is available [here][license]
![preview](docs/assets/preview.png)
![preview](docs/assets/themes.png)
</div>

## Features
- Included own Material You theme with colorizer ([Day](https://t.me/addtheme/materialgram_day), [Dark](https://t.me/addtheme/materialgram_dark))
- **Google Sans** font everywhere (except for Arabic characters, they use the **Vazir** font)
- Material icons instead of default ones
- Added the ability to seek round videos (experimental)
- Added the ability to delete more than 100 messages at once (experimental)
- Added admin menu and chat log buttons above the members list
- Removed message bubble tails and reverted old paddings
- Replaced all sounds
- Reduced image compression
- Reduced minimum window size
- Reduced minimum brush thickness in the photo editor
- Reduced use of uppercase in the interface
- Reduced some timeouts
- Increased upload speed
- Improved spoiler animation
- Improved sticker pack menu. "Share stickers" button replaced with "Remove" + show the amount of stickers near "Add stickers" + ability to copy the author's id
- Improved chat export. 10000 messages in one html document and faster file download
- Show the approximate date of account creation in the profile
- If you would like to see a new feature in this list, please tell me in the [chat](https://t.me/materialgram_chat)

## Build instructions
* [Windows 64-bit][win64]
* [macOS][mac]
* [GNU/Linux using Docker][linux]

## Third-party
* **Qt 6** ([LGPL](http://doc.qt.io/qt-6/lgpl.html)) and **Qt 5.15** ([LGPL](http://doc.qt.io/qt-5/lgpl.html)) slightly patched
* **OpenSSL 1.1.1 and 1.0.1** ([OpenSSL License](https://www.openssl.org/source/license.html))
* **WebRTC** ([New BSD License](https://github.com/desktop-app/tg_owt/blob/master/LICENSE))
* **zlib 1.2.11** ([zlib License](http://www.zlib.net/zlib_license.html))
* **LZMA SDK 9.20** ([public domain](http://www.7-zip.org/sdk.html))
* **liblzma** ([public domain](http://tukaani.org/xz/))
* **Google Breakpad** ([License](https://chromium.googlesource.com/breakpad/breakpad/+/master/LICENSE))
* **Google Crashpad** ([Apache License 2.0](https://chromium.googlesource.com/crashpad/crashpad/+/master/LICENSE))
* **GYP** ([BSD License](https://github.com/bnoordhuis/gyp/blob/master/LICENSE))
* **Ninja** ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* **OpenAL Soft** ([LGPL](https://github.com/kcat/openal-soft/blob/master/COPYING))
* **Opus codec** ([BSD License](http://www.opus-codec.org/license/))
* **FFmpeg** ([LGPL](https://www.ffmpeg.org/legal.html))
* **Guideline Support Library** ([MIT License](https://github.com/Microsoft/GSL/blob/master/LICENSE))
* **Range-v3** ([Boost License](https://github.com/ericniebler/range-v3/blob/master/LICENSE.txt))
* **Vazir font** ([SIL Open Font License 1.1](https://github.com/rastikerdar/vazir-font/blob/master/OFL.txt))
* **Emoji alpha codes** ([MIT License](https://github.com/emojione/emojione/blob/master/extras/alpha-codes/LICENSE.md))
* **Catch test framework** ([Boost License](https://github.com/philsquared/Catch/blob/master/LICENSE.txt))
* **xxHash** ([BSD License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE))
* **QR Code generator** ([MIT License](https://github.com/nayuki/QR-Code-generator#license))
* **CMake** ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* **Hunspell** ([LGPL](https://github.com/hunspell/hunspell/blob/master/COPYING.LESSER))

[//]: # (LINKS)
[telegram_api]: https://core.telegram.org
[telegram_proto]: https://core.telegram.org/mtproto
[license]: LICENSE
[win64]: docs/building-win-x64.md
[mac]: docs/building-mac.md
[linux]: docs/building-linux.md
