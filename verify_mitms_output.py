#!/usr/bin/env python3
import argparse
import pathlib
import re
import subprocess
import sys


CONTROL_RE = re.compile(r"^C\((\d+)\)$")


def parse_searches(searches_path: pathlib.Path):
    lines = searches_path.read_text().splitlines()
    entries = []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        parts = line.split()
        if len(parts) < 2:
            i += 1
            continue
        try:
            n_qubits = int(parts[1])
        except ValueError:
            i += 1
            continue
        if i + 1 + n_qubits > len(lines):
            break
        rows = [lines[i + 1 + j].strip().split() for j in range(n_qubits)]
        entries.append((parts[0], rows))
        i += 1 + n_qubits
    return entries


def load_labels_and_rows(bench: pathlib.Path):
    searches = bench / "searches"
    if not searches.exists():
        raise FileNotFoundError(f"missing {searches}")
    entries = parse_searches(searches)
    labels = [lab for lab, _ in entries]
    rows_by_label = {lab: rows for lab, rows in entries}

    labels_path = bench / "labels.txt"
    if labels_path.exists():
        label_lines = [ln.strip() for ln in labels_path.read_text().splitlines() if ln.strip()]
        if label_lines:
            labels = label_lines

    return labels, rows_by_label


def token_to_qc(token: str):
    if token == "H":
        return "H"
    if token == "T":
        return "T"
    if token == "T*":
        return "T*"
    if token == "S":
        return "P"
    if token == "S*":
        return "P*"
    if token in ("X", "Y", "Z"):
        return token
    return None


def stage_to_qc_lines(row_tokens, stage_idx):
    a, b = row_tokens[0][stage_idx], row_tokens[1][stage_idx]
    if a == "I" and b == "I":
        return []

    controls = []
    target = None
    tokens = [a, b]

    for q, tok in enumerate(tokens):
        m = CONTROL_RE.match(tok)
        if m:
            controls.append(q)
            tgt = int(m.group(1)) - 1
            if target is None:
                target = tgt
            elif target != tgt:
                raise ValueError(f"conflicting controls at stage {stage_idx}")
        elif tok == "X":
            if target is None:
                target = q
            elif target != q:
                raise ValueError(f"multiple X targets at stage {stage_idx}")
        elif tok == "I":
            continue
        else:
            # Single-qubit gate on this qubit.
            pass

    if controls:
        if target is None:
            raise ValueError(f"control without target at stage {stage_idx}")
        if target in controls:
            raise ValueError(f"control targets itself at stage {stage_idx}")
        # Emit a Toffoli with controls then target last.
        qubits = " ".join([f"q{c}" for c in controls] + [f"q{target}"])
        return [f"tof {qubits}"]

    lines = []
    for q, tok in enumerate(tokens):
        if tok == "I":
            continue
        if tok.startswith("C("):
            raise ValueError(f"control without X at stage {stage_idx}")
        gate = token_to_qc(tok)
        if gate is None:
            raise ValueError(f"unsupported token {tok} at stage {stage_idx}")
        lines.append(f"{gate} q{q}")
    return lines


def rows_to_qc(rows):
    if len(rows) != 2:
        raise ValueError("only 2-qubit circuits are supported")
    if len(rows[0]) != len(rows[1]):
        raise ValueError("row lengths differ")

    out = [".v q0 q1", "BEGIN"]
    for t in range(len(rows[0])):
        out.extend(stage_to_qc_lines(rows, t))
    out.append("END")
    return "\n".join(out) + "\n"


def run_mitms_search(
    mitms_bin: pathlib.Path,
    mitms_dir: pathlib.Path,
    label: str,
    threads: int,
    timeout_s: int,
    use_serialize: bool,
):
    cmd = [str(mitms_bin)]
    if not use_serialize:
        cmd.append("-no-serialize")
    if threads > 0:
        cmd += ["-threads", str(threads)]
    cmd.append(label)

    try:
        r = subprocess.run(
            cmd,
            cwd=str(mitms_dir),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_s,
        )
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"

    if r.returncode != 0:
        return False, f"exit={r.returncode} stderr={r.stderr[:120]!r}"

    if "No circuit" in r.stdout:
        return False, "No circuit found"

    if "ERROR:" in r.stdout or "ERROR:" in r.stderr:
        return False, "MITMS error"

    return True, "OK"


def run_mitms_matrix(mitms_bin: pathlib.Path, mitms_dir: pathlib.Path, qc_text: str, timeout_s: int):
    cmd = [str(mitms_bin), "-matrix"]
    try:
        r = subprocess.run(
            cmd,
            cwd=str(mitms_dir),
            input=qc_text,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_s,
        )
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"

    if r.returncode != 0:
        return False, f"exit={r.returncode} stderr={r.stderr[:120]!r}"

    if "ERROR:" in r.stdout or "ERROR:" in r.stderr:
        return False, "MITMS error"

    return True, "OK"


def main():
    ap = argparse.ArgumentParser(
        description="Run mitms on a benchset searches file and report which labels succeed."
    )
    ap.add_argument("benchset_dir", help="benchset directory with searches/labels.txt")
    ap.add_argument("mitms_dir", help="mitms directory containing the mitms binary")
    ap.add_argument("--timeout", type=int, default=30, help="per-label timeout in seconds")
    ap.add_argument("--threads", type=int, default=1, help="mitms -threads value (0 to omit)")
    ap.add_argument("--max", type=int, default=0, help="only test the first N labels")
    ap.add_argument(
        "--mode",
        choices=("matrix", "search"),
        default="matrix",
        help="matrix: call mitms -matrix on a .qc conversion; search: run exact search by label",
    )
    ap.add_argument(
        "--serialize",
        action="store_true",
        help="allow mitms to load/store serialized libraries in search mode",
    )

    args = ap.parse_args()

    bench = pathlib.Path(args.benchset_dir).resolve()
    mitms_dir = pathlib.Path(args.mitms_dir).resolve()

    mitms_bin = mitms_dir / "mitms"
    if not mitms_bin.exists():
        print(f"missing binary: {mitms_bin}", file=sys.stderr)
        return 2

    searches_src = bench / "searches"
    if not searches_src.exists():
        print(f"missing searches file: {searches_src}", file=sys.stderr)
        return 2

    labels, rows_by_label = load_labels_and_rows(bench)
    if not labels:
        print("no labels found", file=sys.stderr)
        return 2

    if args.max > 0:
        labels = labels[: args.max]

    if args.mode == "search":
        (mitms_dir / "searches").write_text(searches_src.read_text())

    ok = 0
    for lab in labels:
        if args.mode == "search":
            good, msg = run_mitms_search(
                mitms_bin,
                mitms_dir,
                lab,
                args.threads,
                args.timeout,
                args.serialize,
            )
        else:
            rows = rows_by_label.get(lab)
            if rows is None:
                print(f"{lab}: FAIL (missing rows)")
                continue
            try:
                qc_text = rows_to_qc(rows)
            except ValueError as exc:
                print(f"{lab}: FAIL ({exc})")
                continue
            good, msg = run_mitms_matrix(mitms_bin, mitms_dir, qc_text, args.timeout)
        if good:
            ok += 1
            print(f"{lab}: OK")
        else:
            print(f"{lab}: FAIL ({msg})")

    print(f"passed {ok}/{len(labels)}")
    return 0 if ok == len(labels) else 1


if __name__ == "__main__":
    raise SystemExit(main())
