#!/usr/bin/env python3
"""Update the plugin hyperparams file and OUTPUTROOT in options.json of a study."""
import argparse
import json
from pathlib import Path


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("study_dir", type=Path)
    p.add_argument("outputroot")
    p.add_argument("max_constraints_per_micro_it", type=int)
    p.add_argument("add_N_constraint", type=lambda s: s.lower() in ("1", "true", "yes"))
    p.add_argument("tol_N", type=float)
    p.add_argument("tol_N_K", type=float)
    a = p.parse_args()

    # Path hardcoded in the plugin (micro_iters_cpp_plugin.cpp), relative to the study dir
    params_path = a.study_dir / "plugin_inputs" / "plugin_params.json"
    params = {
        "max_constraints_per_micro_it": a.max_constraints_per_micro_it,
        "add_N_constraint": a.add_N_constraint,
        "tol_N": a.tol_N,
        "tol_N_K": a.tol_N_K,
    }
    params_path.write_text(json.dumps(params, indent=2) + "\n")

    options_path = a.study_dir / "options.json"
    options = json.loads(options_path.read_text())
    options["OUTPUTROOT"] = a.outputroot
    options_path.write_text(json.dumps(options, indent=4) + "\n")

    print(f"wrote {params_path}: {params}")
    print(f"set OUTPUTROOT={a.outputroot} in {options_path}")


if __name__ == "__main__":
    main()
