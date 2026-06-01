# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Canonical instructions

**`AGENTS.md` is the canonical repository-wide guide — read it first.** It is authoritative on:

- Build system layout (`../Libraries`, `../win64/Libraries`, `../ThirdParty`) and build commands
- The critical "stop on PDB/EXE lock errors, do not retry the build" rule
- Coding style (no descriptive comments, prefer `auto`)
- API usage (TL schema, `api().request(...)` pattern, `.match()`/`.data()`, flood/406 handling)
- UI styling (`.style` files, never hardcode dimensions — always `st::`)
- Localization (`lang.strings`, `tr::` immediate vs reactive, rich-text projectors)
- RPL reactive programming (producers, `rpl::combine`/`merge`, pipeline starters)
- Local storage serialization rules (append-only `QDataStream`)

**`REVIEW.md` is the canonical style/formatting guide** — mechanical rules (empty line before closing brace, operators at start of continuation lines, if-with-initializer, etc.). Consult it before writing or reviewing code.

Do not duplicate those documents here; this file only adds the big-picture orientation they omit.

## Project context

This is **opengram**, a fork of [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) with Material Design and other changes (see `README.md` for the feature list, `changelog.txt` for history). It is a C++/Qt 6 (and Qt 5.15) desktop application built with CMake. Because it tracks upstream tdesktop, prefer to follow existing upstream patterns and keep fork-specific changes minimal and localized.

## Repository layout

- `Telegram/SourceFiles/` — the application source (see module map below)
- `Telegram/lib_*` — in-tree libraries built as separate CMake targets: `lib_base`, `lib_ui`, `lib_rpl`, `lib_tl` (TL codegen), `lib_storage`, `lib_lottie`, `lib_webrtc`, `lib_webview`, `lib_spellcheck`, `lib_translate`, `lib_qr`, `lib_crl` (concurrency runtime)
- `Telegram/Resources/langs/lang.strings` — localization keys
- `Telegram/SourceFiles/mtproto/scheme/{api,mtproto}.tl` — API/protocol schemas (codegen source)
- `Telegram/build/version` — version info
- `docs/building-{win-x64,mac,linux}.md` — platform build instructions
- `lib/` (and `.gitmodules`) — third-party submodules

## Architecture map (`Telegram/SourceFiles/`)

Understanding these layers requires reading across several files; this is the orientation:

- **Entry / app lifecycle**: `main.cpp` → `core/launcher.*` → `core/application.*` (the single `Core::Application`, app-wide state, windows, `Core::Settings` in `core/core_settings.*`).
- **Accounts & sessions**: `main/main_domain.*` owns all accounts; `main/main_account.*` is one account; `main/main_session.*` (`Main::Session`) is the active logged-in session and the root for per-account services. `main/main_session_settings.*` is per-session config. Multi-account is first-class.
- **Network / protocol**: `mtproto/` implements MTProto. `apiwrap.*` (`ApiWrap`, reached via `session().api()`) is the high-level request facade over the generated `MTP...` types from the `.tl` schemas.
- **Data model**: `data/` (≈170 files) holds the in-memory domain model — `data_session.*` (`Data::Session`, the per-session data owner), peers (users/chats/channels), messages, media, etc. Most features read/write through `Data::Session`.
- **UI sections** (each a major screen/feature area): `history/` (chat message list & composing), `dialogs/` (chat list), `info/` (profiles & shared media), `settings/`, `calls/`, `chat_helpers/` (stickers, emoji, inline results), `media/` (player & viewer), `boxes/` (modal dialogs), `intro/` (login), `overview/`, `editor/` (photo editor), `export/`, `payments/`, `passport/`, `statistics/`, `iv/` (instant view).
- **Windowing & chrome**: `window/` — `Window::Controller`/`SessionController`, section widgets, themes (`window/themes/`), notifications, adaptive layout.
- **Platform abstraction**: `platform/` with `win/`, `mac/`, `linux/` implementations behind common headers.
- **Shared UI**: most reusable widgets live in `Telegram/lib_ui`; app-specific UI in `SourceFiles/ui/`.

## Tests

C++ unit tests use the Catch framework and live next to their subjects (e.g. `Telegram/SourceFiles/tests/test_text.cpp`, with `*_tests.cpp` files throughout `lib_*` and `SourceFiles`). They are registered as CTest tests via CMake. After configuring/building, run them through `ctest` from the build directory (run a single test with `ctest -R <name>`). Tests are typically not exercised for routine feature work — see `AGENTS.md` for the Debug-only build guidance.
