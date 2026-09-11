# Asset Sources

Retrieved 2026-09-10 from the projects below.

## PunPun branding

Source: https://github.com/pumpumlang/punpun
License: MIT (`licenses/punpun/LICENSE`)

Bundled:
- `assets/punpun-logo.svg`
- `assets/punpun-logo-dark.svg`
- `assets/punpun-logo-light.svg`
- `assets/punpun-mark.svg`

## Microsoft VS Code Codicons

Source: https://github.com/microsoft/vscode-codicons
Asset paths: `src/icons/*.svg`
Code/icon license included as: `licenses/codicons/LICENSE-CODE`

Bundled:
- `add.svg`
- `close.svg`
- `debug-alt.svg`
- `folder.svg`
- `folder-opened.svg`
- `run-all.svg`
- `save.svg`
- `search.svg`
- `source-control.svg`
- `terminal.svg`

## Tabler Icons

Source: https://github.com/tabler/tabler-icons
Asset paths: `icons/outline/*.svg`
License: MIT (`licenses/tabler/LICENSE`)

Bundled:
- `bug.svg`
- `device-floppy.svg`
- `file-code.svg`
- `git-branch.svg`
- `package.svg`
- `player-play.svg`
- `settings.svg`
- `terminal-2.svg`

## Catppuccin Palette

Source: https://github.com/catppuccin/palette
Reference path: `docs/css.md`
License: MIT (`licenses/catppuccin/LICENSE`)

The CSS reference is bundled as an upstream integration reference. It points to the official `@catppuccin/palette` CSS package for actual palette variables.

## Good candidates deliberately not bundled yet

- JetBrains Mono: https://github.com/JetBrains/JetBrainsMono (OFL-1.1)
- Inter: https://github.com/rsms/inter (OFL-1.1)

They are excellent IDE/UI font candidates, but are omitted from this first ZIP so this starter remains small and focused on reusable visual/UI assets.

## v0.4.2 native UI refresh

### Microsoft Fluent UI System Icons

Source: https://github.com/microsoft/fluentui-system-icons
License: MIT (`licenses/Fluent-UI-System-Icons-MIT.txt`)

Used for native IDE chrome including Explorer, Search, and Run controls. The
vectors are dynamically tinted by the active theme where appropriate.

### Material Icon Theme

Source: https://github.com/material-extensions/vscode-material-icon-theme
License: MIT (`licenses/Material-Icon-Theme-MIT.txt`)

Used for colorful Explorer file-type icons for C, C++, headers, Markdown, JSON,
JavaScript, and Python. PunPun files continue to use the PunPun brand mark.
