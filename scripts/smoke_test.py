#!/usr/bin/env python3
"""Small end-to-end correctness test."""

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data" / "smoke.csv"

subprocess.run([
    "python3", str(ROOT / "tools" / "generate_logs.py"),
    "--lines", "10000", "--output", str(DATA), "--seed", "42"
], check=True, cwd=ROOT)

subprocess.run([
    "python3", str(ROOT / "scripts" / "verify.py"), str(DATA)
], check=True, cwd=ROOT)

print("SMOKE TEST PASSED")
