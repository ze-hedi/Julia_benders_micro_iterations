#!/usr/bin/env bash
# Usage: run_benchmark.sh OUTPUTROOT MAX_CONSTRAINTS_PER_MICRO_IT ADD_N_CONSTRAINT TOL_N TOL_N_K
# Env overrides: STUDY_DIR, BENDERS_EXE, NP
set -euo pipefail

if [ "$#" -ne 5 ]; then
    echo "Usage: $0 OUTPUTROOT MAX_CONSTRAINTS_PER_MICRO_IT ADD_N_CONSTRAINT TOL_N TOL_N_K" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STUDY_DIR="${STUDY_DIR:-/home/bouchehdahed/code/nextep_julia_study_scripts_0926/study_files/THT400_reduced_10-25_401inc_454mon_microit}"
BENDERS_EXE="${BENDERS_EXE:-$HOME/code/antares-xpansion/_build/benders}"
NP="${NP:-10}"

python3 "$SCRIPT_DIR/update_configs.py" "$STUDY_DIR" "$@"

# The plugin resolves ./plugin_inputs/... relative to the working directory
cd "$STUDY_DIR"
mpirun -n "$NP" "$BENDERS_EXE" options.json
