# PunPun IDE 0.5 Native Preview

PunPun IDE is a native desktop IDE focused on PunPun 1.3+, C, C++, headers and Markdown. The editor/runtime is C++20 + Qt 6. Python is build/release tooling only and is not in the finished application's hot path.

Version 0.5 is the first pass aimed at feeling like an editor rather than a collection of Qt widgets that accidentally share a window.

## Highlights in 0.5

- VS Code-inspired workbench: activity bar, Explorer/Search/Run/Extensions sidebars, editor tabs, collapsible Problems/Output/Terminal panel, status bar and a persistent right-side **Code Assistant**.
- Code Assistant combines real compiler/LSP diagnostics with a small offline Local Review layer. It shows error/warning/hint counts, source, diagnostic code, likely cause and a suggested next step. Compiler diagnostics remain authoritative.
- Diagnostics render as gutter markers and wave underlines. Hovering a squiggle shows the diagnostic and suggestion directly at the code.
- A caret-line highlight drawn from the active theme, which yields while a selection is active so selected text never reads as a floating box. Hover cards are dismissed while selecting/dragging and matching brackets use local highlights.
- Expanded highlighting for PunPun, C, C++, headers, Markdown, JSON, JavaScript and Python. PunPun `str`/`String`, builtins, declarations, functions, modules, properties, constants and escapes have distinct token roles.
- `Shift+Alt+F` formats the document. PunPun uses PPC's real `fmt`; C/C++/headers use `clang-format` when installed.
- Live C/C++ checking analyzes the unsaved editor buffer using the host compiler with `-fsyntax-only`. PunPun uses PPC/LSP for live semantics and `ppc check --json` for explicit checks.
- Integrated terminal keeps shell cwd/export state, preserves command history, displays partial prompts immediately, and sends input directly to a running child program instead of accidentally treating stdin as another shell command.
- Explorer right-click: **New PunPun File, New File, New Folder, Rename, Delete**.
- PPX package-manager GUI and native compatibility bridge.
- Automatic stable PunPun toolchain updates. The IDE follows GitHub `releases/latest`, downloads into private app data, verifies SHA-256, smoke-tests PPC, then switches the IDE/LSP/terminal to the verified toolchain.
- Bundled PunPun-native environment doctor reports platform, cwd, PATH, `PPC_RUNTIME`, `PPC_STDLIB`, `CC` and `CXX` through PunPun's own `std.system` API.

## Install

### Linux — download and double-click

Download `PunPun-IDE-v<version>-x86_64.AppImage` from the
[latest release](https://github.com/pumpumlang/punpun-ide/releases/latest),
mark it executable once, and open it:

```sh
chmod +x PunPun-IDE-v*-x86_64.AppImage
./PunPun-IDE-v*-x86_64.AppImage
```

The AppImage carries Qt, libarchive and their dependencies, so nothing else
needs installing. Most desktops run it on a double-click once the executable
bit is set; some ask the first time.

To build one yourself:

```sh
./scripts/build_appimage.sh
```

### Windows — installer

Download and run `PunPun-IDE-<version>-win64.exe` from the same release. It
installs to Program Files with Start-menu and desktop shortcuts and a matching
uninstaller. `PunPun-IDE-v<version>-windows-x64.zip` is the portable
alternative: unpack it and double-click `bin\punpun-ide.exe`.

### PunPun toolchain

The IDE runs and checks PunPun through `ppc` from the
[PunPun toolchain](https://github.com/pumpumlang/punpun). Put `ppc` on `PATH`,
or let the built-in updater fetch the latest stable release. Without it the
editor, C/C++ support and terminal still work; `pp doctor` and the IDE's
environment panel report what is missing.

## Editing

| Shortcut | Action |
| --- | --- |
| `F5` | Run the current file |
| `Ctrl+Shift+B` | Check the current file |
| `Ctrl+/` | Toggle line comment |
| `Tab` / `Shift+Tab` | Indent / outdent the selected lines |
| `Ctrl+Shift+D` | Duplicate selection or line |
| `Alt+Up` / `Alt+Down` | Move the selected lines |
| `Ctrl+Space` | Trigger completion |
| ``Ctrl+` `` | Toggle the terminal |

Indentation carries across Enter and steps in after a block opens (`:` for
PunPun, `{` for C-likes). Brackets and quotes close as you type, typing a
closer steps over it, and backspace between a fresh pair removes both. Every
one of these is switchable under **Settings → Editor**.

## Architecture

The languages have specific jobs instead of being mixed together for decoration:

- **C++20 + Qt 6**: windowing, editor, Explorer, terminal, Run/Check/Debug, LSP, package manager, settings and updater.
- **C11**: stable native plugin/file-type ABI in `c_api/punpun_plugin_api.h`.
- **JavaScript**: sandboxed language-extension metadata/snippets through `QJSEngine`. Scripts are not handed filesystem or shell objects.
- **Python 3**: source audit, tests and release packaging only.

## Run on CachyOS / Arch

Extract the source and run:

```fish
./run.sh
```

First run installs only missing native build dependencies, configures/builds, then launches. Later runs launch the existing binary immediately when sources are unchanged.

Useful commands:

```fish
./run.sh --rebuild      # clean rebuild
./run.sh --test         # build if needed, run tests, then launch
./run.sh --no-launch    # build only
```

No Python virtual environment is required to run the finished IDE.

## Keyboard defaults

| Action | Shortcut |
| --- | --- |
| Open file | `Ctrl+O` |
| Open folder | `Ctrl+K, Ctrl+O` |
| Save | `Ctrl+S` |
| Find | `Ctrl+F` |
| Trigger completion | `Ctrl+Space` |
| Format document | `Shift+Alt+F` |
| Run | `F5` |
| Debug | `Ctrl+F5` |
| Check | `Ctrl+Shift+B` |
| Terminal | `Ctrl + backtick` |
| Code Assistant | `Ctrl+Shift+A` |

Shortcuts are editable in Settings.

## PunPun integration

PPC remains the semantic source of truth. The IDE does not maintain a second PunPun compiler.

- language service: `ppc serve --stdio`
- standalone run: `ppc go file.pp` (`go` and `run` are PPC aliases)
- explicit diagnostics: `ppc check --json file.pp`
- formatting: `ppc fmt file.pp`
- environment probe: bundled `resources/punpun/doctor.pp`

The same PunPun-native doctor is also maintained in the PunPun repository under `tools/ide/doctor.pp`.

## Toolchain updates

The IDE checks the official latest stable GitHub release at startup and at the configured interval, 10 minutes by default. Updates are installed privately and activated only after digest verification and a `ppc --version` smoke test. The user's system installation is not overwritten.

## Assets

PunPun branding is used for `.pp`. Microsoft Fluent UI System Icons provide app chrome, and Material Icon Theme provides colorful C/C++/header/Markdown/JSON/JavaScript/Python file icons. Source links and license notices are under `resources/`.

## Current validation boundary

This source package contains strict non-Qt ABI/bridge tests plus a 75-check source/resource regression audit. A complete GUI compile/render still requires a machine with the Qt 6 development SDK. `./run.sh` is the intended CachyOS test path.
