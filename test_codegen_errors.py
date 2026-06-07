#!/usr/bin/env python3

import os
import subprocess
import sys
import tempfile


def run_vrdg(content):
    with tempfile.NamedTemporaryFile(mode="w", suffix=".cpp", delete=False) as f:
        f.write(content)
        tmp = f.name
    try:
        result = subprocess.run(
            [sys.executable, "vrdg.py", tmp],
            capture_output=True,
            text=True,
        )
        return result.returncode, result.stdout + result.stderr
    finally:
        os.unlink(tmp)


def check(description, returncode, output, expect_exit_nonzero, expect_text):
    ok = True
    if expect_exit_nonzero and returncode == 0:
        print(f"  [FAIL] {description}: expected non-zero exit, got 0")
        ok = False
    if not expect_exit_nonzero and returncode != 0:
        print(f"  [FAIL] {description}: expected exit 0, got {returncode}")
        ok = False
    if expect_text and expect_text not in output:
        print(f"  [FAIL] {description}: expected '{expect_text}' in output")
        ok = False
    if ok:
        print(f"  [PASS] {description}")
    return ok


def run_tests():
    success = True

    print("Testing vrdg.py error paths...\n")

    # Unclosed task at end of file
    rc, out = run_vrdg("///>Graphics_Task(MyTask)\n")
    success &= check(
        "Unclosed task at EOF aborts",
        rc, out,
        expect_exit_nonzero=True,
        expect_text="missing End_Task",
    )

    # End_Task without open
    rc, out = run_vrdg("///>End_Task\n")
    success &= check(
        "End_Task without open task aborts",
        rc, out,
        expect_exit_nonzero=True,
        expect_text="End_Task without matching",
    )

    # Task header missing name
    rc, out = run_vrdg("///>Graphics_Task()\n///>End_Task\n")
    success &= check(
        "Task with empty name aborts",
        rc, out,
        expect_exit_nonzero=True,
        expect_text="task header missing name",
    )

    # New task opened before previous was closed
    rc, out = run_vrdg(
        "///>Graphics_Task(A)\n"
        "///>Graphics_Task(B)\n"
        "///>End_Task\n"
    )
    success &= check(
        "Nested task open aborts",
        rc, out,
        expect_exit_nonzero=True,
        expect_text="opens before previous task",
    )

    # Duplicate GPU_Profile name
    rc, out = run_vrdg(
        "///>Graphics_Task(A)\n"
        "///>GPU_Profile(SameName)\n"
        "///>End_Task\n"
        "///>Graphics_Task(B)\n"
        "///>GPU_Profile(SameName)\n"
        "///>End_Task\n"
    )
    success &= check(
        "Duplicate GPU_Profile name aborts",
        rc, out,
        expect_exit_nonzero=True,
        expect_text="Duplicate GPU_Profile name",
    )

    # Valid minimal task succeeds
    rc, out = run_vrdg("///>Graphics_Task(GoodTask)\n///>End_Task\n")
    success &= check(
        "Valid task exits successfully",
        rc, out,
        expect_exit_nonzero=False,
        expect_text=None,
    )

    print()
    if success:
        print("[PASS] All codegen error tests passed!")
    else:
        print("[FAIL] Some codegen error tests failed!")
    return success


if __name__ == "__main__":
    sys.exit(0 if run_tests() else 1)
