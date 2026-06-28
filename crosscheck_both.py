#!/usr/bin/env python3
import os, re, subprocess, sys, pathlib

TOK = re.compile(r'^(I|H|S\*?|T\*?|X|Y|Z|C\([0-9,]+\))$')

def extract_first_solution(stdout: str) -> str:
    """
    Pull the first contiguous block of stage lines (2 tokens each).
    """
    lines = stdout.splitlines()
    block = []
    in_block = False
    for ln in lines:
        toks = ln.strip().split()
        ok = (len(toks) == 2 and all(TOK.match(t) for t in toks))
        if ok:
            block.append(ln.strip())
            in_block = True
        else:
            if in_block and block:
                break
    return "\n".join(block) + ("\n" if block else "")

def run(cmd, timeout=30, cwd=None, input_text=None):
    return subprocess.run(
        cmd, cwd=cwd, input=input_text, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=timeout
    )

def main():
    if len(sys.argv) != 5:
        print("usage: crosscheck_both.py <benchset_dir> <mitms_dir> <exact_synth_dir> <timeout_s>", file=sys.stderr)
        sys.exit(2)

    bench = pathlib.Path(sys.argv[1]).resolve()
    mitms_dir = pathlib.Path(sys.argv[2]).resolve()
    exact_dir = pathlib.Path(sys.argv[3]).resolve()
    timeout_s = int(sys.argv[4])

    labels = (bench / "labels.txt").read_text().splitlines()
    if not labels:
        print("no labels found", file=sys.stderr)
        sys.exit(2)

    # Required binaries
    mitms_bin = mitms_dir / "mitms"
    qc_to_so6 = exact_dir / "qc_to_so6"
    ours = exact_dir / "mitm_match_tester"  # or your custom tool; change if needed

    for p in [mitms_bin, qc_to_so6, ours]:
        if not p.exists():
            print(f"missing binary: {p}", file=sys.stderr)
            sys.exit(2)

    # Copy searches into mitms dir so it can find labels
    (mitms_dir / "searches").write_text((bench / "searches").read_text())

    ok_count = 0
    for lab in labels:
        target_mat = (bench / "targets" / f"{lab}.mat").read_text().strip()

        # 1) Amy compile
        try:
            r = run([str(mitms_bin), "-threads", "1", lab], timeout=timeout_s, cwd=str(mitms_dir))
        except subprocess.TimeoutExpired:
            print(f"{lab}: mitms TIMEOUT")
            continue

        if r.returncode != 0:
            print(f"{lab}: mitms exit={r.returncode} stderr={r.stderr[:120]!r}")
            continue

        sol = extract_first_solution(r.stdout)
        if not sol.strip():
            print(f"{lab}: could not parse solution circuit from mitms output")
            continue

        # 2) Compute SO6 of Amy’s returned circuit
        rr = run([str(qc_to_so6)], timeout=timeout_s, cwd=str(exact_dir), input_text=sol)
        if rr.returncode != 0:
            print(f"{lab}: qc_to_so6 failed: {rr.stderr[:120]!r}")
            continue
        amy_mat = rr.stdout.strip()

        # 3) Compare matrices (exact string compare; both produced by print_mathematica)
        if amy_mat != target_mat:
            print(f"{lab}: MISMATCH")
            print("  target:", target_mat[:120], "...")
            print("  amy   :", amy_mat[:120], "...")
            continue

        # 4) Your solver must find a meet for this target
        try:
            ours_run = run([str(ours), "--search-depth=6", "--threads=1", f"--target={target_mat}"],
                           timeout=timeout_s, cwd=str(exact_dir))
        except subprocess.TimeoutExpired:
            print(f"{lab}: ours TIMEOUT")
            continue

        if ours_run.returncode != 0:
            print(f"{lab}: ours exit={ours_run.returncode} stderr={ours_run.stderr[:120]!r}")
            continue

        ok_count += 1
        print(f"{lab}: OK")

    print(f"passed {ok_count}/{len(labels)}")

if __name__ == "__main__":
    main()

