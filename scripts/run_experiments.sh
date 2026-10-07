#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NS3_ROOT="${NS3_ROOT:-/home/harsha/ns-3-dev}"
SIM_TIME="${SIMULATION_TIME:-60}"
SEEDS="${SEEDS:-1 2 3}"
RATES=(0.00 0.05 0.10 0.20 0.30)

if [[ ! -x "$NS3_ROOT/ns3" || ! -f "$NS3_ROOT/build/include/ns3/lr-wpan-helper.h" ]]; then
  echo "NS-3 development tree with LR-WPAN headers not found: $NS3_ROOT" >&2
  exit 2
fi
echo "Using $(git -C "$NS3_ROOT" describe --tags --always 2>/dev/null || echo 'NS-3 source tree')"
cmake -S "$ROOT" -B "$ROOT/build" -DNS3_ROOT="$NS3_ROOT"
cmake --build "$ROOT/build" -j"${JOBS:-2}"

BIN="$ROOT/build/wban-anomaly-low-energy"
mkdir -p "$ROOT/results/csv/runs" "$ROOT/results/csv/node-energy" "$ROOT/results/raw" "$ROOT/results/plots"
rm -f "$ROOT/results/csv/runs"/*.csv

for rate in "${RATES[@]}"; do
  rate_tag="${rate/./p}"
  for mode in baseline proposed; do
    for seed in $SEEDS; do
      summary="$ROOT/results/csv/runs/${mode}_a${rate_tag}_seed${seed}.csv"
      raw="$ROOT/results/raw/${mode}_a${rate_tag}_seed${seed}.csv"
      node_energy="$ROOT/results/csv/node-energy/${mode}_a${rate_tag}_seed${seed}.csv"
      echo "Running mode=$mode anomalyRate=$rate seed=$seed simulationTime=$SIM_TIME"
      "$BIN" --mode="$mode" --anomalyRate="$rate" --simulationTime="$SIM_TIME" \
        --seed="$seed" --output="$summary" --rawOutput="$raw" --nodeEnergyOutput="$node_energy"
    done
  done
done

python3 - "$ROOT/results/csv" <<'PY'
import csv, glob, os, sys
out = sys.argv[1]
files = glob.glob(os.path.join(out, 'runs', '*.csv'))
rows = []
for path in sorted(files):
    with open(path, newline='') as f:
        rows.extend(csv.DictReader(f))
if not rows:
    raise SystemExit('No simulation rows found')
fields = list(rows[0])
for mode in ('baseline', 'proposed'):
    dest = os.path.join(out, f'{mode}_results.csv')
    with open(dest, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields); w.writeheader()
        w.writerows(r for r in rows if r['mode'] == mode)
with open(os.path.join(out, 'comparison_results.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
print(f'Combined {len(rows)} actual simulation rows into {out}')
PY

echo "Experiment suite complete. CSV files: $ROOT/results/csv"
