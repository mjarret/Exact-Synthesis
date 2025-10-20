#!/usr/bin/env python3
import argparse
import json
import os
import re
import shutil
import sys
from pathlib import Path

INCLUDE_PAT = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]")
HDR_EXTS = {".h", ".hpp", ".hh", ".hxx", ".inl"}
SRC_EXTS = {".c", ".cc", ".cpp", ".cxx"}

DEFAULT_EXCLUDE_DIRS = {
    ".git", "backup", "docs", "benchmarks", "include/boost", "include/tbb",
    "include/indicators", "include/indicators/details"
}

def load_compile_commands(cc_path: Path):
    if not cc_path.exists():
        return []
    try:
        return json.loads(cc_path.read_text())
    except Exception:
        return []

def norm(p: Path) -> Path:
    try:
        return p.resolve()
    except Exception:
        return p

def discover_all_files(root: Path):
    files = []
    for dirpath, dirnames, filenames in os.walk(root):
        rel = Path(dirpath).relative_to(root)
        # skip excluded dirs
        skip = False
        for ex in DEFAULT_EXCLUDE_DIRS:
            if str(rel).startswith(ex):
                skip = True
                break
        if skip:
            continue
        for fn in filenames:
            p = Path(dirpath) / fn
            files.append(norm(p))
    return files

def parse_includes(p: Path) -> list[Path]:
    incs = []
    try:
        txt = p.read_text(errors="ignore")
    except Exception:
        return incs
    basedir = p.parent
    for line in txt.splitlines():
        m = INCLUDE_PAT.match(line)
        if not m:
            continue
        inc = m.group(1)
        cand = (basedir / inc)
        if cand.exists():
            incs.append(norm(cand))
            continue
        # try project-root relative
        root_cand = Path.cwd() / inc
        if root_cand.exists():
            incs.append(norm(root_cand))
            continue
        # try include/ prefix
        inc_cand = Path.cwd() / "include" / inc
        if inc_cand.exists():
            incs.append(norm(inc_cand))
            continue
    return incs

def is_header(p: Path) -> bool:
    return p.suffix in HDR_EXTS

def is_source(p: Path) -> bool:
    return p.suffix in SRC_EXTS

def index_by_name(files: list[Path]) -> dict[str, list[Path]]:
    idx: dict[str, list[Path]] = {}
    for p in files:
        idx.setdefault(p.name, []).append(p)
    return idx

def parse_makefile_src(makefile: Path) -> list[Path]:
    if not makefile.exists():
        return []
    srcs: list[Path] = []
    try:
        txt = makefile.read_text()
    except Exception:
        return srcs
    # naive parse of lines like: SRC := file1.cpp file2.cpp ...
    for line in txt.splitlines():
        if line.strip().startswith("SRC :="):
            parts = line.split(":=", 1)[1].strip().split()
            for part in parts:
                p = Path(part)
                if p.suffix in SRC_EXTS and p.exists():
                    srcs.append(norm(p))
    return srcs

def collect_roots(args) -> set[Path]:
    roots: set[Path] = set()
    # From compile_commands.json
    all_files = discover_all_files(Path.cwd())
    name_index = index_by_name(all_files)
    cc = load_compile_commands(Path("compile_commands.json"))
    for entry in cc:
        f = entry.get("file")
        if not f:
            continue
        p = Path(f)
        if p.exists():
            roots.add(norm(p))
        else:
            # try to resolve by filename only
            cands = name_index.get(p.name, [])
            for c in cands:
                roots.add(norm(c))
    # tests
    if args.include_tests:
        for p in Path("tests").glob("**/*.cpp"):
            roots.add(norm(p))
    # benches
    if args.include_bench:
        for p in Path("apps").glob("**/*bench*.cpp"):
            roots.add(norm(p))
        for p in Path("benchmarks").glob("**/*.cpp"):
            roots.add(norm(p))
    # parse Makefile SRC as roots
    for p in parse_makefile_src(Path("Makefile")):
        roots.add(norm(p))
    # ensure main exists
    mp = Path("apps/main.cpp")
    if mp.exists():
        roots.add(norm(mp))
    # custom roots
    for r in args.roots:
        rp = Path(r)
        if rp.is_dir():
            for p in rp.glob("**/*"):
                if p.is_file() and (is_source(p) or is_header(p)):
                    roots.add(norm(p))
        elif rp.is_file():
            roots.add(norm(rp))
    return roots

def reachable_from(roots: set[Path]) -> set[Path]:
    used: set[Path] = set()
    stack = [*roots]
    while stack:
        p = stack.pop()
        if p in used:
            continue
        used.add(p)
        if p.suffix in HDR_EXTS.union(SRC_EXTS):
            for dep in parse_includes(p):
                if dep not in used:
                    stack.append(dep)
    return used

def main():
    ap = argparse.ArgumentParser(description="Dead code pruner (headers and sources)")
    ap.add_argument("--apply", action="store_true", help="Delete unused files (default: dry-run)")
    ap.add_argument("--include-tests", action="store_true", default=True, help="Keep tests as roots (default: on)")
    ap.add_argument("--include-bench", action="store_true", default=True, help="Keep benches as roots (default: on)")
    ap.add_argument("--roots", nargs="*", default=[], help="Additional root files/dirs")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    root = Path.cwd()
    all_files = discover_all_files(root)
    all_files_set = set(all_files)

    roots = collect_roots(args)
    used = reachable_from(roots)

    # Any source not in compile_commands/tests/bench is unused
    unused_files: list[Path] = []
    for p in all_files:
        if not (is_source(p) or is_header(p) or p.suffix == ".inl"):
            continue
        # Exclude third-party trees
        rel = p.relative_to(root)
        if any(str(rel).startswith(ex) for ex in DEFAULT_EXCLUDE_DIRS):
            continue
        if p not in used:
            unused_files.append(p)

    if not unused_files:
        print("[deadcode] No unused headers or sources detected.")
        return 0

    print("[deadcode] Candidates ({}):".format(len(unused_files)))
    for p in sorted(unused_files):
        print(" -", p)

    if args.apply:
        backup = Path(".deadcode.backup")
        with backup.open("w") as bf:
            for p in unused_files:
                bf.write(str(p) + "\n")
        for p in unused_files:
            try:
                os.remove(p)
            except Exception as e:
                print(f"[deadcode] Failed to remove {p}: {e}")
        print(f"[deadcode] Removed {len(unused_files)} files. Backup list at {backup}")
    else:
        print("[deadcode] Dry-run only. Use --apply to remove above files.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
