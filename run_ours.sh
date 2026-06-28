#!/usr/bin/env bash
set -euo pipefail

BIN="${BIN:-./mitm_match_tester}"
DEPTH="${DEPTH:-8}"      # per-side depth; 8 covers total distance up to 16
THREADS="${THREADS:-1}"
TIMEOUT_S="${TIMEOUT_S:-60}"
SETDIR="${1:-benchset}"

echo "label,exit,dl,dr,tcount,wall_s,rss_kb" > ours.csv
mkdir -p logs_ours

to_seconds() {
python3 - <<'PY'
import sys
t=sys.stdin.read().strip()
if not t: print(""); sys.exit(0)
p=t.split(":")
try:
  if len(p)==3: h,m,s=p; print(float(s)+60*int(m)+3600*int(h))
  elif len(p)==2: m,s=p; print(float(s)+60*int(m))
  else: print(float(p[0]))
except: print("")
PY
}

while IFS= read -r label; do
  [[ -z "$label" ]] && continue
  matfile="$SETDIR/targets/$label.mat"
  out="logs_ours/$label.out"
  err="logs_ours/$label.err"
  mat="$(cat "$matfile")"

  set +e
  /usr/bin/time -v timeout "$TIMEOUT_S" \
    "$BIN" --search-depth="$DEPTH" --threads="$THREADS" --target="$mat" >"$out" 2>"$err"
  code=$?
  set -e

  rss_kb=$(grep -i 'Maximum resident set size' "$err" | awk '{print $6}' | tail -n1)
  wall=$(grep -i 'Elapsed (wall clock) time' "$err" | sed 's/.*: //' | tail -n1)
  wall_s=$(printf "%s" "$wall" | to_seconds)

  # Parse dl/dr from stdout: "dl = X, dr = Y" :contentReference[oaicite:3]{index=3}
  dl=$(grep -oE 'dl = [0-9]+' "$out" | head -n1 | awk '{print $3}' || true)
  dr=$(grep -oE 'dr = [0-9]+' "$out" | head -n1 | awk '{print $3}' || true)
  if [[ -n "${dl:-}" && -n "${dr:-}" ]]; then
    tcount=$((dl + dr))
  else
    tcount=""
  fi

  echo "$label,$code,${dl:-},${dr:-},${tcount:-},${wall_s:-},${rss_kb:-}" >> ours.csv
done < "$SETDIR/labels.txt"
