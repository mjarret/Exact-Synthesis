#!/usr/bin/env python3
import re, subprocess, pathlib, sys

TOK = re.compile(r'^[A-Z*]+$')  # T, S, H, I, T*, S* etc.

def extract_two_rows(mitms_stdout: str):
    lines = mitms_stdout.splitlines()
    rows = []
    for ln in lines:
        # lines like: " T   S   I   I  "
        toks = [t for t in ln.strip().split() if t]
        if toks and all(TOK.match(t) for t in toks):
            rows.append(toks)
            if len(rows) == 2:
                break
    if len(rows) != 2 or len(rows[0]) != len(rows[1]):
        return None
    return rows[0], rows[1]

def main():
    if len(sys.argv) != 5:
        print("usage: check_one.py <mitms_dir> <exact_dir> <benchset_dir> <label>", file=sys.stderr)
        return 2

    mitms_dir = pathlib.Path(sys.argv[1]).resolve()
    exact_dir = pathlib.Path(sys.argv[2]).resolve()
    bench_dir = pathlib.Path(sys.argv[3]).resolve()
    label = sys.argv[4]

    mitms = mitms_dir / "mitms"
    qc_to_so6 = exact_dir / "qc_to_so6"   # build from the C++ I gave earlier
    target_path = bench_dir / "targets" / f"{label}.mat"

    target = target_path.read_text().strip()

    r = subprocess.run([str(mitms), "-threads", "1", label],
                       cwd=str(mitms_dir), text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("mitms failed:", r.returncode, r.stderr)
        return 1

    rows = extract_two_rows(r.stdout)
    if rows is None:
        print("could not parse two-row circuit from mitms output")
        print(r.stdout[:400])
        return 1

    row0, row1 = rows
    # feed stage lines to qc_to_so6: one line per stage, "g0 g1"
    stages = "\n".join(f"{row0[t]} {row1[t]}" for t in range(len(row0))) + "\n"
    rr = subprocess.run([str(qc_to_so6)],
                        cwd=str(exact_dir), text=True,
                        input=stages, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if rr.returncode != 0:
        print("qc_to_so6 failed:", rr.stderr[:200])
        return 1

    amy_mat = rr.stdout.strip()

    print("target == amy?", target == amy_mat)
    if target != amy_mat:
        print("target:", target[:120], "...")
        print("amy   :", amy_mat[:120], "...")
        return 1

    print("OK")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

