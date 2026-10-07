#!/usr/bin/env python3
import os, subprocess, tempfile, json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data" / "sample.csv"
BIN = ROOT / "bin"

def run(cmd, env=None):
    p = subprocess.run(cmd, cwd=ROOT, env=env, text=True,
                       capture_output=True)
    if p.returncode:
        print(p.stdout)
        print(p.stderr)
        raise SystemExit(p.returncode)
    print(p.stdout.strip())

with tempfile.TemporaryDirectory() as td:
    seq = Path(td) / "seq.json"
    omp = Path(td) / "omp.json"
    hyb = Path(td) / "hyb.json"

    run([str(BIN/"sequential"), str(DATA), str(seq)])
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = "2"
    run([str(BIN/"openmp"), str(DATA), str(omp)], env)
    run(["mpirun", "--oversubscribe", "-np", "2",
         str(BIN/"hybrid"), str(DATA), str(hyb)], env)

    def norm(p):
        d = json.loads(p.read_text())
        d.pop("elapsed_ms", None)
        return d

    assert norm(seq) == norm(omp), "Sequential and OpenMP differ"
    assert norm(seq) == norm(hyb), "Sequential and hybrid differ"
    print("SMOKE TEST PASSED: all three implementations agree.")
