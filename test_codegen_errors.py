#!/usr/bin/env python3

import os
import re
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
        return result
    finally:
        os.unlink(tmp)


def check(description, result, expect_exit_nonzero, expect_text):
    returncode = result.returncode
    output = result.stdout + result.stderr
    ok = True
    if expect_exit_nonzero and result.stdout:
        print(f"  [FAIL] {description}: generator error entered stdout")
        ok = False
    if "Traceback" in result.stderr:
        print(f"  [FAIL] {description}: unhandled Python exception")
        ok = False
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
    result = run_vrdg("///>Graphics_Task(MyTask)\n")
    success &= check(
        "Unclosed task at EOF aborts",
        result,
        expect_exit_nonzero=True,
        expect_text="missing End_Task",
    )

    # End_Task without open
    result = run_vrdg("///>End_Task\n")
    success &= check(
        "End_Task without open task aborts",
        result,
        expect_exit_nonzero=True,
        expect_text="End_Task without matching",
    )

    # Task header missing name
    result = run_vrdg("///>Graphics_Task()\n///>End_Task\n")
    success &= check(
        "Task with empty name aborts",
        result,
        expect_exit_nonzero=True,
        expect_text="task header missing name",
    )

    # New task opened before previous was closed
    result = run_vrdg(
        "///>Graphics_Task(A)\n"
        "///>Graphics_Task(B)\n"
        "///>End_Task\n"
    )
    success &= check(
        "Nested task open aborts",
        result,
        expect_exit_nonzero=True,
        expect_text="opens before previous task",
    )

    # Duplicate GPU_Profile name
    result = run_vrdg(
        "///>Graphics_Task(A)\n"
        "///>GPU_Profile(SameName)\n"
        "///>End_Task\n"
        "///>Graphics_Task(B)\n"
        "///>GPU_Profile(SameName)\n"
        "///>End_Task\n"
    )
    success &= check(
        "Duplicate GPU_Profile name aborts",
        result,
        expect_exit_nonzero=True,
        expect_text="Duplicate GPU_Profile name",
    )

    # Resource declarations and access intent must be unambiguous.
    invalid_resources = [
        ("Read cannot discard", "Read_TextureFB(tex, Discard)", "Discard is only valid"),
        ("Read/write cannot discard", "ReadWrite_Buffer(buf, Discard)", "Discard is only valid"),
        ("Unknown write intent", "Write_Buffer(buf, Maybe)", "expected one access role"),
        ("Missing access name", "Write_Buffer()", "invalid resource access"),
        ("Conflicting read and discard", "Read_Buffer(buf)\n///>Write_Buffer(buf, Discard)", "conflicting discard access"),
        ("Conflicting preserving and discard writes", "Write_Buffer(buf)\n///>Write_Buffer(buf, Discard)", "conflicting discard access"),
        ("Conflicting resource types", "Read_Buffer(buf)\n///>Read_TextureFB(buf)", "conflicting resource types"),
        ("Invalid lifetime", "Lifetime(tex, Forever)", "Lifetime requires"),
        ("Undeclared lifetime", "Lifetime(tex, Persistent)", "Lifetime requires a resource declared"),
        ("Duplicate lifetime", "Lifetime(tex, Persistent)\n///>Lifetime(tex, Imported)", "duplicate Lifetime"),
        ("Incomplete buffer descriptor", "Create_Buffer(buf, 4, 0)", "Create_Buffer requires"),
        ("Incomplete texture descriptor", "Create_TextureFB(tex, 16, 16, 0)", "Create_TextureFB requires"),
        ("Duplicate declaration", "Create_Buffer(buf, 4, 0, 0)\n///>Create_Buffer(buf, 4, 0, 0)", "duplicate resource declaration"),
    ]
    for description, body, diagnostic in invalid_resources:
        result = run_vrdg("///>Graphics_Task(A)\n///>" + body + "\n///>End_Task\n")
        success &= check(description, result, True, diagnostic)

    invalid_directives = [
        ("Unknown directive", "Read_Bufer(buf)", "unknown directive"),
        ("Exact directive name", "FuncExtra(callback)", "unknown directive"),
        ("Missing Func argument", "Func()", "requires one nonempty argument"),
        ("Extra profile argument", "CPU_Profile(A, B)", "requires one nonempty argument"),
        ("Duplicate Func", "Func(A)\n///>Func(B)", "duplicate Func"),
        ("Missing parenthesis", "Func(callback", "missing closing parenthesis"),
        ("Trailing text", "Func(callback) ignored", "unexpected text"),
        ("End_Task arguments", "End_Task(ignored)", "takes no arguments"),
    ]
    for description, body, diagnostic in invalid_directives:
        result = run_vrdg("// first line\n///>Graphics_Task(A)\n///>" + body + "\n///>End_Task\n")
        success &= check(description, result, True, diagnostic)
        success &= check(description + " includes source and task", result, True, (":4:" if "\n" in body else ":3:") + " error: task 'A':")
        success &= check(description + " includes annotation", result, True, "[" + body.split("\n///>")[-1] + "]")

    result = run_vrdg("///>Func(callback)\n")
    success &= check("Func outside a task", result, True, ":1: error: directive outside task")
    result = run_vrdg("///>Graphics_Task(A)\n///>End_Task\n///>Graphics_Task(A)\n///>End_Task\n")
    success &= check("Duplicate task includes first location", result, True, "duplicate task 'A', first declared at")
    result = run_vrdg("// first line\n///>Graphics_Task(A)\n///>End_Task\n")
    success &= check("Task source metadata", result, False, None)
    if not re.search(r'task\.source = "[^/\\"]+\.cpp:2";', result.stdout):
        print("  [FAIL] Task metadata must contain only the filename and task header line")
        success = False

    with tempfile.TemporaryDirectory() as directory:
        headers = []
        for tree in ("private build a", "private build b"):
            source_dir = os.path.join(directory, tree)
            os.mkdir(source_dir)
            source = os.path.join(source_dir, "passes.cpp")
            with open(source, "w", encoding="utf-8") as f:
                f.write("// first line\n///>Graphics_Task(A)\n///>End_Task\n")
            result = subprocess.run(
                [sys.executable, "vrdg.py", source], capture_output=True, text=True
            )
            success &= check("Absolute input emits filename only", result, False, 'task.source = "passes.cpp:2";')
            headers.append(result.stdout)
        if headers[0] != headers[1] or directory in "".join(headers):
            print("  [FAIL] Generated headers must not depend on local directory paths")
            success = False

    invalid_roles = [
        ("Sampled buffer", "Read_Buffer(buf, Sampled)"),
        ("Indirect texture", "Read_TextureFB(tex, Indirect)"),
        ("Sampled write", "Write_TextureFB(tex, Sampled)"),
        ("Indirect write", "Write_Buffer(buf, Indirect)"),
        ("Color read", "Read_TextureFB(tex, Color)"),
        ("Copy read/write", "ReadWrite_Buffer(buf, Copy)"),
        ("Compute attachment", "Write_TextureFB(tex, Color)"),
        ("Compute depth", "Read_TextureFB(tex, Depth)"),
    ]
    for description, body in invalid_roles:
        task = "Compute_Task" if description.startswith("Compute") else "Graphics_Task"
        result = run_vrdg("///>" + task + "(A)\n///>" + body + "\n///>End_Task\n")
        success &= check(description, result, True, "invalid access role")
    for body, diagnostic in [
        ("Read_Buffer(buf, Storage, Discard)", "Discard is only valid"),
        ("Write_Buffer(buf, Discard, Storage)", "expected one access role"),
        ("Write_TextureFB(tex, Color)\n///>Write_TextureFB(tex, Storage)", "conflicting access roles"),
        ("Read_Buffer(buf, Copy)\n///>Write_Buffer(buf, Copy)", "invalid access role"),
    ]:
        result = run_vrdg("///>Graphics_Task(A)\n///>" + body + "\n///>End_Task\n")
        success &= check("Conflicting typed access", result, True, diagnostic)
    result = run_vrdg("///>Graphics_Task(A)\n///>Read_Buffer(buf, Storage)\n///>Read_Buffer(buf, Indirect)\n///>End_Task\n")
    success &= check("Compatible named read roles merge", result, False, "VRDG_READ | VRDG_STORAGE | VRDG_INDIRECT")

    # Valid minimal task succeeds
    result = run_vrdg("///>Graphics_Task(GoodTask)\n///>End_Task\n")
    success &= check(
        "Valid task exits successfully",
        result,
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
