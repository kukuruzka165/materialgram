# opengram plugins

This directory holds the Python sidecar runtime for the in-app plugin bridge
(`Telegram/SourceFiles/plugins`).

- `opengram_plugins/` — the sidecar package (stdlib-only). It is the module the
  C++ `Bridge` launches via `python -m opengram_plugins.sidecar <dir>`.
- `examples/` — sample `.plg` plugins (`hello.plg`, `boom.plg`).

## How it ships

`Telegram/CMakeLists.txt` copies `opengram_plugins/` and `examples/` next to the
built executable (a `plugins/` folder beside `Telegram.exe`). On first run the
bridge seeds them into the user profile (`<workingdir>/plugins`), so plugins work
on a fresh profile without any manual setup.

## Bundling Python (no system Python required)

The bridge resolves its interpreter in this order (`plugins_bridge.cpp`,
`PythonExecutable()`):

1. A bundled runtime next to the executable:
   - Windows: `python/python.exe`
   - other: `python/bin/python3`
2. System `python` / `python3` on `PATH` (fallback).

To make plugins work for users without Python, drop a self-contained interpreter
into a `python/` folder beside the executable during packaging:

- **Windows**: extract the official *Windows embeddable package* zip from
  python.org into `python/` (so `python/python.exe` exists). The sidecar is
  stdlib-only, so no extra wheels are needed.

This step is intentionally left to the packaging/CI job (the binary is large and
platform-specific); the C++ side already prefers it automatically when present.

## ExteraGram `.plugin` files

opengram parses ExteraGram-style metadata headers (`__name__`, `__author__`,
`__version__`, `__description__`) so these files display correctly in the install
box. **They do not execute** — ExteraGram plugins target the Android client's
JVM/Dalvik runtime and Xposed-style Java method hooking, which has no equivalent
in this C++/Qt desktop app.
