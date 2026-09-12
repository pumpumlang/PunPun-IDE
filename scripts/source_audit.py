#!/usr/bin/env python3
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
required = [
    "CMakeLists.txt", "run.sh", "src/MainWindow.cpp", "src/MainWindow.h",
    "src/AssistantPanel.cpp", "src/AssistantPanel.h", "src/SmartAnalyzer.cpp", "src/SmartAnalyzer.h",
    "src/PunPunLanguage.h",
    "src/CodeEditor.cpp", "src/CodeEditor.h", "src/SyntaxHighlighter.cpp", "src/SyntaxHighlighter.h",
    "src/TerminalWidget.cpp", "src/TerminalWidget.h", "src/ToolchainManager.cpp", "src/ToolchainManager.h",
    "src/LspClient.cpp", "src/LspClient.h", "src/ScriptHost.cpp", "src/ScriptHost.h",
    "c_api/punpun_plugin_api.c", "c_api/punpun_plugin_api.h",
    "resources/js/builtin-extension.js", "resources/punpun/doctor.pp", "resources/resources.qrc",
]
missing = [item for item in required if not (root / item).exists()]
if missing:
    raise SystemExit("Missing required files: " + ", ".join(missing))

main = (root / "src/MainWindow.cpp").read_text()
editor = (root / "src/CodeEditor.cpp").read_text()
highlighter = (root / "src/SyntaxHighlighter.cpp").read_text()
terminal = (root / "src/TerminalWidget.cpp").read_text()
# Comment text must not satisfy or break code-shape checks.
terminal_code = "\n".join(line for line in terminal.splitlines()
                          if not line.lstrip().startswith("//"))
theme = (root / "src/Theme.cpp").read_text()
toolchain = (root / "src/ToolchainManager.cpp").read_text()
lsp = (root / "src/LspClient.cpp").read_text()
assistant = (root / "src/AssistantPanel.cpp").read_text()
analyzer = (root / "src/SmartAnalyzer.cpp").read_text()
language = (root / "src/PunPunLanguage.h").read_text()
qrc_path = root / "resources/resources.qrc"
qrc = qrc_path.read_text()
doctor = (root / "resources/punpun/doctor.pp").read_text()
cmake = (root / "CMakeLists.txt").read_text()
version = (root / "VERSION").read_text().strip()

checks = {
    "version synchronized": f"project(PunPunIDE VERSION {version}" in cmake,
    "v0.5 sources registered": "AssistantPanel.cpp" in cmake and "SmartAnalyzer.cpp" in cmake,
    "right-side assistant": "new AssistantPanel" in main and "refreshAssistantForCurrent" in main,
    "assistant severity counters": "assistantErrors" in assistant and "assistantWarnings" in assistant,
    "live smart checks": "SmartAnalyzer::analyze" in main and "runNativeSyntaxCheck(editor, false)" in main,
    "unsaved C/C++ buffer checking": '"-fsyntax-only"' in main and "process->write(source)" in main,
    "compiler diagnostics JSON": '{"check", "--json", path}' in main,
    # `ppc run` compiles and runs a PunPun file; the call also passes the file's
    # own directory so the program can open files by relative path.
    "PunPun standalone run": 'runCommand(ppc, {"run", path}, cwd)' in main,
    # Run bailed silently when the compiler was missing, which read as "the Run
    # button is broken". Every refusal has to reach the terminal.
    "run reports why it did not run":
        "reportRunProblem" in main and "showNotice" in terminal,
    # PunPun's installer puts ppc in ~/.local/bin and extends PATH from the
    # shell profile, which a desktop launch never reads. Discovery cannot rely
    # on PATH alone or the IDE finds nothing.
    "toolchain discovery beyond PATH":
        '.local/bin' in toolchain and "searchPaths" in toolchain,
    "PunPun formatter": '{"fmt", path}' in main,
    "header is not executable": "Header files are checked, not executed" in main,
    "environment doctor resource": "PUNPUN_IDE_DOCTOR_V1" in doctor and "punpun/doctor.pp" in qrc,
    "doctor std.system": "import std.system" in doctor and "system_platform()" in doctor,
    # The caret-line band is welcome, but it must never paint while a selection
    # is active: that is what made selected text look like a floating box.
    "selection hover fix": "hoverDismissRequested" in editor,
    "caret line yields to selection":
        "highlightLine_ && !textCursor().hasSelection()" in editor,
    "matching brackets": "matching delimiters near the caret" in editor,
    "rich highlighting": "propertyFmt_" in highlighter and "escapeFmt_" in highlighter,
    # One vocabulary table, shared by the highlighter, the completer and the
    # analyzer, transcribed from the compiler. Three private copies had drifted
    # into painting `trait`/`impl` and recommending `bring` over `import`.
    "str highlighting": '"str",' in language and '"String"' in language,
    "shared language table":
        "PunPunLanguage" in highlighter and "PunPunLanguage" in analyzer,
    "modern import is not reported as legacy":
        '{"bring", "import"' in language and '{"import"' not in language,
    "terminal interactive input": "foregroundBusy_" in terminal and "Program input" in terminal,
    "terminal partial prompts": "Show prompts/output even when programs do not print" in terminal,
    # QString::arg() leaves "%%" alone, so the shell protocol must spell the
    # printf conversion as a bare %s or the cwd report returns literal text.
    "terminal cwd format is a real conversion":
        '%%s' not in terminal_code and "printf '%4%s" in terminal_code,
    "terminal command protocol": "__PPIDE_BEGIN__" in terminal and "__PPIDE_END__" in terminal,
    "terminal stop": "stopForeground" in terminal,
    "neutral status bar": "QStatusBar { background:@panel" in theme,
    "blue selection": "#264f78" in theme,
    "latest stable updater": "releases/latest" in toolchain,
    "PPC LSP": re.search(r'process_\.start\(ppcPath,\s*\{\s*"serve",\s*"--stdio"\s*\}\s*\)', lsp) is not None,
    "Qt6 file-dialog regression": not re.search(r"QFileDialog::get(?:Open|Save)FileName\([^;]+?\)\.first\b", main),
    "QMenuBar complete type": "#include <QMenuBar>" in main,
    "QPushButton complete type": "#include <QPushButton>" in main and "#include <QPushButton>" in assistant,
    "QPixmap complete type": "#include <QPixmap>" in (root / "src/IconUtils.cpp").read_text(),
    "CodeEditor direct Qt types": all(token in editor for token in ("#include <QFrame>", "#include <QFont>", "#include <QTextCharFormat>")),
    "no Python runtime API": not any("Python.h" in p.read_text(errors="ignore") for p in (root / "src").glob("*.cpp")),
}

# Every CMake source/header path named under src/c_api/resources must exist.
for match in re.finditer(r"(?m)^\s*((?:src|c_api|resources)/[^\s)]+)", cmake):
    rel = match.group(1)
    checks[f"CMake path exists: {rel}"] = (root / rel).exists()

# Validate resources and ensure every qrc entry points to a real file.
tree = ET.parse(qrc_path)
for node in tree.findall(".//file"):
    rel = (node.text or "").strip()
    checks[f"resource exists: {rel}"] = bool(rel) and (root / "resources" / rel).exists()

# Regression guard for the unreadable one-line-if style that produced piles of
# misleading-indentation warnings in the 0.4 builds. Limit it to critical files
# where we already cleaned the code, rather than rejecting normal compact code.
for name, text in {"MainWindow.cpp": main, "LspClient.cpp": lsp, "ToolchainManager.cpp": toolchain}.items():
    bad = re.search(r"\bif[ \t]*\([^\n]+\)[ \t]*[^\n{;]+;[ \t]+[A-Za-z_]", text)
    checks[f"no misleading one-line ifs: {name}"] = bad is None

failed = []
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
    if not ok:
        failed.append(name)
if failed:
    raise SystemExit("Source audit failed: " + ", ".join(failed))

for path in root.joinpath("resources").rglob("*.svg"):
    ET.parse(path)
print("PASS: resource XML")
print(f"Source audit passed ({len(checks)} checks)")
