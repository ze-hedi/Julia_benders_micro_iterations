#!/usr/bin/env bash
# Run every analysis script on one study and put all outputs under results/<study>/.
# Usage: ./run_analysis.sh [study_dir ...]     (default: 100_constraint)
#        PYTHON=python3.11 ./run_analysis.sh   to pick the interpreter
set -euo pipefail
cd "$(dirname "$0")"

PY="${PYTHON:-python3}"
if ! "$PY" -c "import matplotlib" 2>/dev/null; then
    echo "matplotlib is missing for $PY: run '$PY -m pip install matplotlib' first" >&2
    exit 1
fi

studies=("$@")
[ ${#studies[@]} -eq 0 ] && studies=(100_constraint)

for study in "${studies[@]}"; do
    study="${study%/}"
    [ -d "$study/added_constraints" ] || { echo "skip $study: no added_constraints/ folder" >&2; continue; }
    out="results/$study"
    echo "== $study -> $out"
    "$PY" analysis/added_constraints_ratio.py "$study" "$out/n_vs_nk"
    "$PY" analysis/common_constraints_heatmap.py "$study" "$out/common_between_subproblems"
    "$PY" analysis/common_constraints_across_iterations.py "$study" "$out/common_between_iterations"
done
# comparisons across all studies (one run, not per study)
"$PY" analysis/solve_time_by_max_constraints.py results/max_constraints_comparison/solve_time
"$PY" analysis/added_constraints_by_max_constraints.py results/max_constraints_comparison/added_constraints
echo "done: see results/"
