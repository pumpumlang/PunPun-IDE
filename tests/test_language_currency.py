#!/usr/bin/env python3
"""Hold the baked vocabulary to what the installed compiler actually knows.

`src/PunPunLanguage.h` is a transcription, and a transcription goes stale: the
table written against PunPun 1.4 was already missing sixty-six builtins by 1.5.
Staleness in one direction is harmless -- PPC's own completion is merged in over
the top, so a newer PunPun gains its new names without the IDE changing. Offering
something the compiler does *not* have is not harmless: it is the IDE inventing
language, which is the defect this whole table was introduced to end.

So the check is asymmetric. Names the table has and the compiler does not are a
failure. Names the compiler has and the table lacks are reported, so the drift
is visible, but do not fail the build.

Skips when no toolchain is installed; the IDE is expected to build and test
without one.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "PunPunLanguage.h"


def find_ppc():
    """The same places the IDE looks, in the same order."""
    home = Path.home()
    candidates = []
    for env in ("PUNPUN_PREFIX", "PUNPUN_HOME"):
        if os.environ.get(env):
            candidates.append(Path(os.environ[env]) / "bin")
    candidates += [home / ".local/bin", home / ".punpun/bin", home / "bin",
                   Path("/usr/local/bin"), Path("/opt/punpun/bin")]
    for directory in candidates:
        candidate = directory / "ppc"
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return shutil.which("ppc")


def baked_builtins():
    """The contents of `builtins()` in the header."""
    text = HEADER.read_text(encoding="utf-8")
    match = re.search(r"inline QStringList builtins\(\) \{\s*return \{(.*?)\};",
                      text, re.S)
    if not match:
        raise AssertionError("builtins() not found in " + str(HEADER))
    return set(re.findall(r'"([^"]+)"', match.group(1)))


def compiler_builtins(ppc, workdir):
    """Ask the language server what it can complete to."""
    source = "launch {\n    say(x);\n}\n"
    probe = Path(workdir) / "probe.pp"
    probe.write_text(source, encoding="utf-8")

    process = subprocess.Popen([ppc, "serve", "--stdio"],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def send(payload):
        body = json.dumps(payload).encode()
        process.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        process.stdin.flush()

    def receive():
        length = 0
        while True:
            line = process.stdout.readline()
            if not line:
                raise AssertionError("language server closed the connection")
            if line in (b"\r\n", b"\n"):
                break
            if line.lower().startswith(b"content-length:"):
                length = int(line.split(b":")[1])
        return json.loads(process.stdout.read(length))

    try:
        send({"jsonrpc": "2.0", "id": 1, "method": "initialize",
              "params": {"rootUri": None, "capabilities": {}}})
        send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
        send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
              "params": {"textDocument": {"uri": probe.as_uri(), "languageId": "punpun",
                                          "version": 1, "text": source}}})
        send({"jsonrpc": "2.0", "id": 2, "method": "textDocument/completion",
              "params": {"textDocument": {"uri": probe.as_uri()},
                         "position": {"line": 1, "character": 9}}})
        result = None
        for _ in range(20):
            message = receive()
            if message.get("id") == 2:
                result = message.get("result")
                break
    finally:
        process.kill()
        process.wait(timeout=10)
        for pipe in (process.stdin, process.stdout):
            if pipe:
                pipe.close()

    if isinstance(result, dict):
        result = result.get("items", [])
    names = set()
    for item in result or []:
        label = item.get("label") if isinstance(item, dict) else item
        if label:
            names.add(label)
    return names


class LanguageCurrencyTests(unittest.TestCase):
    def test_baked_table_offers_nothing_the_compiler_lacks(self):
        ppc = find_ppc()
        if not ppc:
            self.skipTest("no PunPun toolchain installed; the fallback table "
                          "cannot be checked against a compiler")

        import tempfile
        with tempfile.TemporaryDirectory() as workdir:
            known = compiler_builtins(ppc, workdir)
        self.assertTrue(known, "the language server returned no completions")

        baked = baked_builtins()
        invented = sorted(baked - known)
        self.assertEqual(
            invented, [],
            "src/PunPunLanguage.h offers names this compiler does not have. "
            "Either they were removed from the language or they were never in "
            "it; re-transcribe builtins() from compiler/src/sema/builtins.cpp:\n  "
            + "\n  ".join(invented))

        missing = sorted(known - baked)
        if missing:
            version = subprocess.run([ppc, "--version"], capture_output=True,
                                     text=True).stdout.strip()
            print(f"note: {len(missing)} builtin(s) in {version} are not in the "
                  f"offline fallback table. Completion still offers them while "
                  f"PPC is running; re-transcribe builtins() to cover the "
                  f"no-toolchain case: {', '.join(missing[:12])}"
                  + (" ..." if len(missing) > 12 else ""))


if __name__ == "__main__":
    unittest.main()
