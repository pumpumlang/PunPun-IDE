# Changelog

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
