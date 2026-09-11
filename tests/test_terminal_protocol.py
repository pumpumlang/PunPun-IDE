#!/usr/bin/env python3
"""Smoke-test the POSIX terminal protocol contract used by TerminalWidget.

This deliberately needs only /bin/bash and the stdlib, so it can catch the
important prompt/stdin/cwd behavior without requiring Qt to render a window.
"""
from __future__ import annotations

import os
import select
import subprocess
import time

BEGIN = "__PPIDE_BEGIN__"
END = "__PPIDE_END__"
CWD = "__PPIDE_CWD__"


def read_available(pipe, timeout: float = 1.0) -> str:
    fd = pipe.fileno()
    chunks: list[bytes] = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if not ready:
            if chunks:
                break
            continue
        chunk = os.read(fd, 4096)
        if not chunk:
            break
        chunks.append(chunk)
    return b"".join(chunks).decode(errors="replace")


def main() -> int:
    process = subprocess.Popen(
        ["/bin/bash"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
    )
    assert process.stdin is not None
    assert process.stdout is not None

    command = (
        f"printf '{BEGIN}\\n'; "
        "{ printf 'name? '; read x; printf 'hello:%s\\n' \"$x\"; }; "
        "__ppide_code=$?; "
        f"printf '{END}%s\\n' \"$__ppide_code\"; "
        f"printf '{CWD}%s\\n' \"$PWD\"\n"
    )
    process.stdin.write(command.encode())
    process.stdin.flush()
    first = read_available(process.stdout)
    if BEGIN not in first or "name? " not in first:
        raise SystemExit(f"terminal protocol did not expose partial prompt: {first!r}")

    process.stdin.write(b"tester\n")
    process.stdin.flush()
    second = read_available(process.stdout)
    process.terminate()
    process.wait(timeout=2)

    expected = ["hello:tester", f"{END}0", CWD]
    missing = [item for item in expected if item not in second]
    if missing:
        raise SystemExit(f"terminal protocol missing {missing}: {second!r}")

    print("Terminal protocol test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
