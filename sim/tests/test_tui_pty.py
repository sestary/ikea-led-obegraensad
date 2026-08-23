#!/usr/bin/env python3
"""Runs the simulator under a real pty and checks a whole frame arrives.

This is the only place the O_NONBLOCK-on-a-shared-tty bug shows up: with a
piped stdin, tcgetattr() fails and raw mode is never entered, so the C++ tests
cannot catch it. On a terminal, stdin and stdout are the same open file
description, so making stdin non-blocking also makes stdout non-blocking, and
a 4 KB frame write gets truncated to whatever fits in the tty buffer.
"""

import os
import pty
import select
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "..", "build", "obegraensad-sim")

MATRIX_ROWS = 8  # 16 LED rows drawn two-per-line as half blocks


def run_under_pty(timeout=2.0):
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(BIN, [BIN])
    out = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], 0.2)
        if fd in r:
            try:
                chunk = os.read(fd, 65536)
            except OSError:
                break
            if not chunk:
                break
            out += chunk
            if out.count(b"\x1b[0m") > MATRIX_ROWS * 2:
                break
    try:
        os.write(fd, b"q")
        time.sleep(0.2)
    except OSError:
        pass
    try:
        os.kill(pid, 9)
    except OSError:
        pass
    os.waitpid(pid, 0)
    os.close(fd)
    return out


def main():
    if not os.path.exists(BIN):
        print("SKIP: binary not built")
        return 0

    out = run_under_pty()
    failures = []

    # A frame ends each matrix line with a reset. Fewer than MATRIX_ROWS means
    # the write was truncated part-way through the panel.
    resets = out.count(b"\x1b[0m")
    if resets < MATRIX_ROWS:
        failures.append(
            f"only {resets} matrix rows reached the terminal, expected >= {MATRIX_ROWS} "
            "(frame write truncated)"
        )

    # The footer is emitted last, so its presence proves the whole frame landed.
    if b"q quit" not in out:
        failures.append("footer 'q quit' never arrived (frame write truncated)")

    if b"OBEGR" not in out:
        failures.append("header never arrived")

    if failures:
        print("FAIL test_tui_pty")
        for f in failures:
            print("  -", f)
        print(f"  (captured {len(out)} bytes)")
        return 1

    print(f"run test_tui_pty\n\nall tests passed ({len(out)} bytes captured)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
