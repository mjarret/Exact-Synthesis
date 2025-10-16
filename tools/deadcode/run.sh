#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
cd "$ROOT_DIR"

MODE=${1:-scan}

case "$MODE" in
  scan)
    python3 tools/deadcode/prune_dead_code.py --verbose || true
    ;;
  apply)
    python3 tools/deadcode/prune_dead_code.py --apply --verbose || true
    ;;
  *)
    echo "Usage: $0 [scan|apply]"; exit 1;;
esac

