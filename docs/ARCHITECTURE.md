# Architecture

The 0.4 rewrite removes Python from runtime UI/editor work.

`MainWindow` owns layout/workspace actions. `CodeEditor` owns editing, gutter diagnostics and completion UI. `SyntaxHighlighter` provides cheap lexical highlighting. `LspClient` speaks framed JSON-RPC directly to PPC 1.3. `ToolchainManager` follows the stable GitHub release API and verifies downloads before activating them. `ScriptHost` executes data-only JavaScript extensions in a `QJSEngine` with no exposed host objects. `punpun_plugin_api.c` is the C ABI boundary for lightweight native integrations.

The UI is intentionally custom-composed from splitters/frames instead of QDockWidget chrome so the hierarchy behaves more like a modern editor shell.
