#!/usr/bin/env python3
"""Verify that sequential and OpenMP produce the same analytical result."""

import argparse
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def normalized(path: Path) -> dict:
    with path.open(encoding="utf-8") as f:
        data = json.load(f)
    data.pop("elapsed_ms", None)
    data.pop("analysis_ms", None)
    data.pop("threads", None)
    return data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv")
    args = parser.parse_args()

    input_path = Path(args.input_csv)
    if not input_path.is_absolute():
        input_path = (ROOT / input_path).resolve()

    seq_output = ROOT / "results" / "verify_sequential.json"
    omp_output = ROOT / "results" / "verify_openmp.json"

    subprocess.run([
        str(ROOT / "bin" / "sequential"), str(input_path), str(seq_output)
    ], check=True, cwd=ROOT)

    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = "4"
    subprocess.run([
        str(ROOT / "bin" / "openmp"), str(input_path), str(omp_output)
    ], check=True, cwd=ROOT, env=env)

    if normalized(seq_output) != normalized(omp_output):
        print("VERIFICATION FAILED: sequential and OpenMP results differ.")
        return 1

    print("VERIFICATION PASSED: sequential and OpenMP results are identical.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
