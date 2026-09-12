# Changelog

## 0.5.2

Run works on a normally installed system, and the language intelligence agrees
with the compiler.

### Fixed

- **Run did nothing.** Pressing F5 on a PunPun file produced no output, no
  error and no terminal activity on a machine where `ppc` was installed and
  worked from a shell. Two causes, both fixed:
  - The IDE looked for the toolchain only in its own managed directory and on
    `PATH`. PunPun's installer puts `ppc` in `~/.local/bin` and makes it
    reachable by appending to the shell profile — which a desktop launcher, a
    `.desktop` entry and an AppImage never read. Discovery now covers
    `~/.local/bin`, `~/.punpun/bin`, `/usr/local/bin`, `/opt/punpun/bin`,
    `$PUNPUN_PREFIX`, `$PUNPUN_HOME`, the open project's `build/` directory and
    the Windows install locations, before falling back to `PATH`. The IDE's
    terminal inherits the same list, so a command typed there resolves the
    compiler the Run button uses.
  - When the compiler genuinely was missing, Run returned in silence after
    writing one line to a side panel. Every refusal now opens the terminal and
    says what is missing, where it looked, and the three ways to fix it. The
    same applies to running a header, an unsupported file type, or a C/C++ file
    with no host compiler installed.
- `Settings → Toolchain` sets the directory explicitly when discovery cannot
  find an unusual installation. It takes priority over every other location.
- Double-clicking a file in the Explorer opened it *and* started an inline
  rename on top of it. Renaming stays on the context menu.
- The offline analyzer reported the modern `import` as a mistake and
  recommended `bring`, `pin` and `var`-style bindings — the spellings PPC
  answers with migration warning W2000. The direction is reversed: legacy forms
  are reported, each naming the modern spelling the compiler would.
- Word-level rules no longer match inside strings and comments, so prose
  containing "no", "each" or "give" is not reported as migration-dialect code.
  Ambiguous words are reported only where a statement can begin.
- Findings are no longer duplicated. Where PPC has already reported a line, the
  offline hint for that line stands down.
- Completion offered `trait` and `impl`, which PunPun does not have, along with
  seven migration keywords, and was missing `contract`, `object`, `meets`,
  `import`, `for`/`in` and every builtin beyond a hand-picked sixteen.
- Highlighting covers the 1.4 grammar: `contract`, `object`, `meets`, `init`,
  `sealed`, `self`, `import`, `match`/`case` and the rest. Migration keywords
  are painted in a muted style rather than as ordinary keywords.
- The bundled environment doctor and the settings font preview were written in
  the migration dialect. Both are modern PunPun now — they are the first
  PunPun many people read.
- Run invokes the documented `ppc run` rather than the `go` alias.

### Added

- Completion merges the compiler's symbols with the built-in words instead of
  replacing them. PPC answers `textDocument/completion` with the builtins of
  whichever toolchain is installed -- verified against 1.5.0, whose sixty-six
  new GUI, networking and process builtins appear with no IDE change -- but it
  does not return keywords, so replacing the list dropped `fn`, `let` and
  `contract` until the file was reopened.
- The offline fallback table is transcribed from PunPun 1.5, and
  `tests/test_language_currency.py` compares it against whatever toolchain is
  installed: offering a name the compiler does not have fails, drift the other
  way is reported. It skips when no toolchain is present.
- `src/PunPunLanguage.h`: the keyword, type and builtin tables in one place,
  transcribed from the compiler's `token.hpp` and `builtins.cpp`. The
  highlighter, the completer and the analyzer share it, so they cannot drift
  apart from each other or from PPC again.
- An analyzer test suite pinning the direction of every PunPun rule, and four
  new source-audit checks covering toolchain discovery, Run's reporting, and
  the shared vocabulary.

## 0.5.1

Bug-fix pass over the 0.5 preview.

### Fixed

- Opening any file crashed the IDE. The editor configuration helper called
  itself instead of the editor's own configure, so the first editor recursed
  until the stack ran out.
- Every selection highlight used the wrong colour. The stylesheet carried nine
  numbered markers but was given fourteen values, so the ninth colour landed in
  the last marker and five were dropped. Colours are substituted by name now,
  and the three hardcoded severity colours that made the Light theme unreadable
  follow the theme.
- The terminal reported its working directory as the literal text `%s`. The
  shell protocol doubled a percent sign expecting QString::arg to collapse it;
  arg substitutes numbered placeholders only.
- Running a PunPun file used whatever directory the shell sat in, so programs
  could not open files by relative path. The file's own directory is used, for
  C and C++ output too.
- A background update check reported every network failure as an error, so an
  offline or proxied machine met a red updater error on each launch. Background
  polls stay quiet and retry; only checks you ask for report.
- `punpun-ide --version` tried to open a file named `--version`, because every
  argument was treated as a path.
- Tabs drew Qt's stock close button as a stark red cross that read as an error
  badge.
- Autosave defaulted to on, writing files that had never been saved.

### Editor

The editor had no behaviour beyond inserting spaces for Tab, and Tab with a
selection replaced the selected text. Added, each switchable in Settings:

- indentation that survives Enter and steps in after a block opens
- bracket and quote pairing that steps over a closer, skips mid-word quotes,
  wraps a selection, and removes both halves on backspace
- block indent and outdent that keep the selection
- line comment toggling using the language's own token
- duplicate line/selection, and move lines up and down
- a caret-line highlight that yields while a selection is active
- trailing-whitespace trimming and final-newline insertion on save

### Settings

Editor and file options for all of the above, a Files tab, a live font preview,
a fixed-width-only font list, terminal font size, and readable command names in
the keybinding table instead of raw ids.

### Packaging

- Linux AppImage: one self-contained executable, built by
  `scripts/build_appimage.sh`, no Qt installation required.
- Windows NSIS installer with Start-menu and desktop shortcuts and an
  uninstaller, alongside the portable zip.
- The desktop entry accepts files (`%F`), registers `text/x-punpun` for `.pp`
  and the C/C++/Markdown types, and sets `StartupWMClass`.
- CI builds and smoke-tests the AppImage, and publishes both platforms with
  checksums on a `v*` tag.

### Tests

- Twelve Qt Test cases covering the editor behaviours above.
- The terminal protocol test reimplemented the command string in Python rather
  than reading the C++ that builds it, so it passed while the protocol was
  broken. The source audit now reads the real format string; verified to fail
  when the defect is reintroduced.


## 0.5.0

- Added persistent right-side Code Assistant with compiler/LSP diagnostics, local review, severity counts and jump-to-source navigation.
- Added diagnostic hover cards over editor squiggles.
- Removed the old full-width selection decoration and made hover UI selection-aware.
- Expanded PunPun/C/C++/Markdown/JS/Python/JSON highlighting roles.
- Added live unsaved-buffer C/C++ syntax checks.
- Added PPC JSON checks and PPC-backed PunPun formatting.
- Reworked terminal command protocol so stdin goes to foreground programs and cwd/export state survives normal commands.
- Added bundled PunPun-native environment doctor and integrated environment summary.
- Kept automatic stable PunPun updates with digest verification and PPC smoke testing.
- Expanded source/resource regression audit and cleaned previously compressed C++ hot paths.
