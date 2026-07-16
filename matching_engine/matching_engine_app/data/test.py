#!/usr/bin/env python3

import subprocess
import sys
import os

def run_test(name, input_file, expected_stdout_file, expected_stderr_file=None):
    """Run a test case and verify output."""
    exe = './build/debug/bin/matching_engine_app'
    
    with open(input_file, 'r') as f:
        input_data = f.read()
    
    with open(expected_stdout_file, 'r') as f:
        expected_stdout = f.read()
    
    expected_stderr = ""
    if expected_stderr_file:
        with open(expected_stderr_file, 'r') as f:
            expected_stderr = f.read()
    
    result = subprocess.run(
        [exe],
        input=input_data,
        capture_output=True,
        text=True
    )
    
    stdout_ok = result.stdout == expected_stdout
    stderr_ok = result.stderr == expected_stderr
    
    if stdout_ok and stderr_ok:
        print(f"✓ {name}")
        return True
    else:
        print(f"✗ {name}")
        if not stdout_ok:
            print(f"  STDOUT mismatch:")
            print(f"    Expected: {repr(expected_stdout)}")
            print(f"    Got:      {repr(result.stdout)}")
        if not stderr_ok:
            print(f"  STDERR mismatch:")
            print(f"    Expected: {repr(expected_stderr)}")
            print(f"    Got:      {repr(result.stderr)}")
        return False

if __name__ == "__main__":
    os.chdir('/workspace')
    
    tests = [
        ("Sample from spec", "matching_engine/matching_engine_app/data/sample_input.txt",
         "matching_engine/matching_engine_app/data/expected_stdout.txt",
         "matching_engine/matching_engine_app/data/expected_stderr.txt"),
    ]
    
    passed = 0
    failed = 0
    for test_args in tests:
        if run_test(*test_args):
            passed += 1
        else:
            failed += 1
    
    print(f"\nResults: {passed} passed, {failed} failed")
    sys.exit(0 if failed == 0 else 1)
