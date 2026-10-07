#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "bin")

def run(cmd):
    result = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        raise RuntimeError("Command failed: " + " ".join(cmd))
    return result

def normalized(path):
    with open(path, encoding="utf-8") as f:
        d = json.load(f)

    # Runtime is expected to differ, so remove it.
    d.pop("elapsed_ms", None)
    return d

def verify_dataset(dataset):
    with tempfile.TemporaryDirectory() as td:
        seq = os.path.join(td, "seq.json")
        omp = os.path.join(td, "omp.json")
        hyb = os.path.join(td, "hyb.json")

        run([os.path.join(BIN, "sequential"), dataset, seq])
        run([os.path.join(BIN, "openmp"), dataset, omp])

        # Two processes, two threads/process. On machines with one CPU,
        # change this to -np 1 and OMP_NUM_THREADS=1.
        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = "2"
        result = subprocess.run(
            ["mpirun", "--oversubscribe", "-np", "2",
             os.path.join(BIN, "hybrid"), dataset, hyb],
            cwd=ROOT, env=env, text=True, capture_output=True
        )
        if result.returncode != 0:
            print(result.stdout)
            print(result.stderr, file=sys.stderr)
            raise RuntimeError("MPI verification failed")

        a = normalized(seq)
        b = normalized(omp)
        c = normalized(hyb)

        if a != b:
            print("FAIL: Sequential != OpenMP")
            return False
        if a != c:
            print("FAIL: Sequential != MPI+OpenMP")
            return False

        print(f"PASS: all implementations agree for {os.path.basename(dataset)}")
        return True

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 scripts/verify.py data/log.csv")
        return 1

    dataset = os.path.abspath(sys.argv[1])
    if not os.path.exists(dataset):
        print("Dataset does not exist:", dataset)
        return 1

    ok = verify_dataset(dataset)
    return 0 if ok else 1

if __name__ == "__main__":
    raise SystemExit(main())
