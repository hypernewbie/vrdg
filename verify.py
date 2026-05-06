#!/usr/bin/env python3

import subprocess
import sys
import os
import re


def run_test():
    """Run vrdg_test.exe and verify its output."""
    # Determine executable path
    if os.name == "nt":  # Windows
        exe_path = os.path.join("build", "Debug", "vrdg_test.exe")
    else:
        exe_path = os.path.join("build", "vrdg_test")

    if not os.path.exists(exe_path):
        print(f"Error: Executable not found at {exe_path}")
        print("Please build the project first: cmake --build build")
        return False

    try:
        result = subprocess.run([exe_path], capture_output=True, text=True, timeout=10)
    except subprocess.TimeoutExpired:
        print("Error: Test timed out after 10 seconds")
        return False
    except Exception as e:
        print(f"Error running executable: {e}")
        return False

    output = result.stdout
    success = True
    checks_passed = 0
    checks_total = 0

    # Helper function for checking patterns
    def check_pattern(pattern, description):
        nonlocal checks_passed, checks_total, success
        checks_total += 1
        if re.search(pattern, output, re.MULTILINE):
            print(f"  [PASS] {description}")
            checks_passed += 1
            return True
        else:
            print(f"  [FAIL] {description}")
            success = False
            return False

    # Helper for checking a pattern *inside* a specific [HASH TEST] section, so
    # a stale line from an earlier step can't satisfy a later step's check.
    def check_hash_section(section_header_pattern, sub_pattern, description):
        nonlocal checks_passed, checks_total, success
        checks_total += 1
        m = re.search(section_header_pattern, output)
        if not m:
            print(f"  [FAIL] {description} (section header not found)")
            success = False
            return False
        rest = output[m.end():]
        next_section = re.search(r"\[HASH TEST\]|\[HASH STATISTICS\]|\[STATISTICS\]", rest)
        section_text = rest if not next_section else rest[: next_section.start()]
        if re.search(sub_pattern, section_text):
            print(f"  [PASS] {description}")
            checks_passed += 1
            return True
        else:
            print(f"  [FAIL] {description}")
            success = False
            return False

    def check_header_pattern(header_text, pattern, description):
        nonlocal checks_passed, checks_total, success
        checks_total += 1
        if re.search(pattern, header_text):
            print(f"  [PASS] {description}")
            checks_passed += 1
            return True
        else:
            print(f"  [FAIL] {description}")
            success = False
            return False

    print("Verifying extensive VRDG test output...\n")

    # Check basic structure
    print("Phase 1: Basic Structure")
    check_pattern(r"VRDG Extensive Test Suite", "Test suite header present")
    check_pattern(r"\[COMPILE 1 - Initial graph build\]", "Compile phase 1 present")
    check_pattern(r"Tasks registered: 30", "Correct number of tasks (30)")

    # Check execution phases
    print("\nPhase 2: Execution Phases")
    check_pattern(r"\[RUN 1 - First execution\]", "Run phase 1 present")
    check_pattern(r"\[RUN 2 - Second execution", "Run phase 2 present")
    check_pattern(r"\[RUN 3 - Third execution\]", "Run phase 3 present")
    check_pattern(r"\[COMPILE 2 - Recompile", "Recompile phase present")
    check_pattern(r"\[RUN 4 - Execution after recompile\]", "Run phase 4 present")

    # Check deep chain tasks
    print("\nPhase 3: Deep Chain (10 levels)")
    check_pattern(r"Chain_A", "Chain_A task executed")
    check_pattern(r"Chain_B.*buf_a.*buf_b", "Chain_B reads buf_a, writes buf_b")
    check_pattern(r"Chain_J.*buf_i.*buf_j", "Chain_J reads buf_i, writes buf_j")

    # Check diamond dependencies
    print("\nPhase 4: Diamond Dependencies")
    check_pattern(r"Diamond_1.*buf_j.*buf_k", "Diamond_1 processes buf_j")
    check_pattern(r"Diamond_2.*buf_j.*buf_l", "Diamond_2 processes buf_j")
    check_pattern(r"Diamond_3.*buf_j.*fb_p", "Diamond_3 processes buf_j")
    check_pattern(r"Diamond_4.*buf_j.*fb_q", "Diamond_4 processes buf_j")
    check_pattern(
        r"Diamond_Merge.*buf_k.*buf_l.*fb_p.*fb_q", "Diamond merge combines all inputs"
    )

    # Check compute/graphics mix
    print("\nPhase 5: Compute/Graphics Mix")
    check_pattern(r"Compute_1.*buf_m.*fb_r", "Compute task 1 executes")
    check_pattern(r"Compute_2.*fb_r.*fb_s", "Compute task 2 executes")
    check_pattern(
        r"\[TRANSITION\] FrameBuffer.*READ COMPUTE", "Compute read transition"
    )
    check_pattern(
        r"\[TRANSITION\] FrameBuffer.*WRITE COMPUTE", "Compute write transition"
    )
    check_pattern(r"Graphics_1.*fb_s.*fb_t", "Graphics task executes")

    # Check parallel chains
    print("\nPhase 6: Parallel Chains")
    check_pattern(r"Parallel_A1.*buf_n", "Parallel chain A starts")
    check_pattern(r"Parallel_B1.*buf_o", "Parallel chain B starts")
    check_pattern(r"Parallel_Merge.*fb_v.*fb_x.*fb_t", "Parallel chains merge")

    # Check resource management
    print("\nPhase 7: Resource Management")
    check_pattern(r"\[CREATE\] Buffer buf_a", "Buffer creation occurs")
    check_pattern(r"\[CREATE\] FrameBuffer fb_p", "FrameBuffer creation occurs")
    check_pattern(r"\[RELEASE\] Buffer", "Buffer release occurs")
    check_pattern(r"\[RELEASE\] FrameBuffer", "FrameBuffer release occurs")
    check_pattern(r"\[TRANSITION\] Buffer.*WRITE GRAPHICS", "Graphics write transition")
    check_pattern(r"\[TRANSITION\] Buffer.*READ GRAPHICS", "Graphics read transition")

    # Check read-only tasks
    print("\nPhase 8: Read-Only Tasks")
    check_pattern(r"ReadOnly_1.*fb_y", "ReadOnly task 1 executes")
    check_pattern(r"ReadOnly_2.*fb_y", "ReadOnly task 2 executes")

    # Check statistics
    print("\nPhase 9: Statistics")
    check_pattern(r"\[STATISTICS\][\s\S]*?Total Creations: 100", "100 total creations (25 per run x 4 runs)")
    check_pattern(r"\[STATISTICS\][\s\S]*?Total Releases: 100", "100 total releases (matching creations)")
    check_pattern(r"\[STATISTICS\][\s\S]*?Total Transitions: 324", "Total transitions pinned to 324")
    check_pattern(r"Total execution time: \d+ ms", "Execution time measured")

    # Check hash recompile tests. Each buildCount check is anchored to the
    # specific [HASH TEST] section it belongs to, so a stale line from an
    # earlier step can't satisfy a later step's pass condition.
    print("\nPhase 11: Hash Recompile Tests")
    check_pattern(r"\[HASH RECOMPILE TESTS\]", "Hash recompile test section present")
    check_hash_section(
        r"\[HASH TEST\] Compile A \(config=0\)",
        r"\[HASH\] Build #1 \(config=0\)",
        "First compile with config A triggers Build #1",
    )
    check_hash_section(
        r"\[HASH TEST\] Compile A \(config=0\)",
        r"\[HASH\] buildCount=1, tasks=30",
        "After config A, buildCount=1 / tasks=30",
    )
    check_hash_section(
        r"\[HASH TEST\] Compile A again \(same config\)",
        r"\[HASH\] buildCount=1 \(should be 1\), tasks=30",
        "Re-compile A: no rebuild (buildCount stays at 1)",
    )
    check_hash_section(
        r"\[HASH TEST\] Compile B \(config changed to 1\)",
        r"\[HASH\] Build #2 \(config=1\)",
        "Config B triggers Build #2",
    )
    check_hash_section(
        r"\[HASH TEST\] Compile B \(config changed to 1\)",
        r"\[HASH\] buildCount=2 \(should be 2\), tasks=30",
        "After config B, buildCount=2 / tasks=30",
    )
    check_hash_section(
        r"\[HASH TEST\] Compile B again \(same config\)",
        r"\[HASH\] buildCount=2 \(should be 2\), tasks=30",
        "Re-compile B: no rebuild (buildCount stays at 2)",
    )
    check_hash_section(
        r"\[HASH TEST\] Compile C \(config changed to 2\)",
        r"\[HASH\] Build #3 \(config=2\)",
        "Config C triggers Build #3",
    )
    check_hash_section(
        r"\[HASH TEST\] Compile C \(config changed to 2\)",
        r"\[HASH\] buildCount=3 \(should be 3\), tasks=30",
        "After config C, buildCount=3 / tasks=30",
    )
    check_hash_section(
        r"\[HASH TEST\] Back to A \(config changed to 0\)",
        r"\[HASH\] Build #4 \(config=0\)",
        "Returning to A triggers Build #4 (different from current hash)",
    )
    check_hash_section(
        r"\[HASH TEST\] Back to A \(config changed to 0\)",
        r"\[HASH\] buildCount=4 \(should be 4\), tasks=30",
        "After return to A, buildCount=4 / tasks=30",
    )

    # Run after final rebuild — verifies state was reset cleanly between rebuilds.
    print("\nPhase 11b: Run after final rebuild")
    check_pattern(
        r"\[HASH TEST\] Run on hashState after final rebuild",
        "Run-after-rebuild section present",
    )
    check_pattern(
        r"\[HASH STATISTICS\][\s\S]*?Total Creations: 25",
        "Run after rebuild: 25 creations (single run on cleanly-rebuilt graph)",
    )
    check_pattern(
        r"\[HASH STATISTICS\][\s\S]*?Total Releases: 25",
        "Run after rebuild: 25 releases (matching creations)",
    )

    # Check test completion
    print("\nPhase 12: Test Completion")
    check_pattern(r"\[TEST COMPLETE\]", "Test completed successfully")

    # Check GPU_Profile codegen by reading the generated header directly.
    print("\nPhase 13: GPU Profile Codegen (test.h)")
    header_path = "test.h"
    if not os.path.exists(header_path):
        print(f"  [FAIL] {header_path} not found — cannot verify GPU profile codegen")
        success = False
    else:
        with open(header_path, "r", encoding="utf-8", errors="ignore") as fh:
            header = fh.read()
        check_header_pattern(header, r"VRDG_GPU_PROFILE_BEGIN\(", "VRDG_GPU_PROFILE_BEGIN emitted")
        check_header_pattern(header, r"VRDG_GPU_PROFILE_END\(", "VRDG_GPU_PROFILE_END emitted")
        check_header_pattern(
            header,
            r"s_rgraphGpuProfilePasses\[2\]",
            "GPU profile table sized for 2 entries",
        )
        check_header_pattern(
            header,
            r'\{\s*"GpuProfilePass",\s*\d+ULL\s*\}',
            "GpuProfilePass entry present in table",
        )
        check_header_pattern(
            header,
            r'\{\s*"GpuProfilePass2",\s*\d+ULL\s*\}',
            "GpuProfilePass2 entry present in table",
        )
        check_header_pattern(
            header,
            r"aeRenderGraph_GetGpuProfilePasses\s*\(\s*\)",
            "aeRenderGraph_GetGpuProfilePasses() defined",
        )
        check_header_pattern(
            header,
            r"aeRenderGraph_GetGpuProfilePassCount\s*\(\s*\)[\s\S]*?return 2;",
            "aeRenderGraph_GetGpuProfilePassCount() returns 2",
        )
        # Same name must produce same timer ID across the BEGIN call site and the table.
        begin_ids = re.findall(r"VRDG_GPU_PROFILE_BEGIN\(\s*(\d+)ULL", header)
        table_ids = dict(re.findall(r'\{\s*"([^"]+)",\s*(\d+)ULL\s*\}', header))
        checks_total += 1
        if begin_ids and len(begin_ids) >= 2 and begin_ids[0] == table_ids.get("GpuProfilePass") and begin_ids[1] == table_ids.get("GpuProfilePass2"):
            print("  [PASS] Timer IDs at call-site match table entries (deterministic hashing)")
            checks_passed += 1
        else:
            print(f"  [FAIL] Timer ID mismatch: begin={begin_ids}, table={table_ids}")
            success = False

    print(f"\n{'=' * 60}")
    print(f"Verification Results: {checks_passed}/{checks_total} checks passed")
    print(f"{'=' * 60}")

    if success:
        print("[PASS] Extensive test passed!")
        return True
    else:
        print("[FAIL] Extensive test failed!")
        print("\nFull output:")
        print(output)
        return False


if __name__ == "__main__":
    sys.exit(0 if run_test() else 1)
