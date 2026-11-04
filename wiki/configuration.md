# Configuration

**Summary:** The plugin is tuned through a single JSON file, `plugin_inputs/plugin_params.json`, and consumes grid data plus three CSV dictionaries from fixed paths. This page lists the knobs and indexes all the input files.
**Main code:** `cpp_plugin/gridModelisation.h`, `cpp_plugin/micro_iters_cpp_plugin.cpp`
**Parent:** [Entry](entry.md)
**Related:** [Grid modelisation](grid-modelisation/index.md), [C++ plugin boundary](cpp-plugin.md)

## Purpose

The plugin has no command-line arguments. Its behavior is controlled by `plugin_params.json`, and its data comes from files it resolves relative to the process working directory. Knowing these paths and keys is needed to tune the algorithm or to debug a "file not found" or missing-key error.

## `plugin_params.json`

Read once in `Plugin::load_config` (`cpp_plugin/gridModelisation.h`), at the hard-coded path `./plugin_inputs/plugin_params.json`. All keys are optional; a missing key falls back to its default, and a missing file prints the defaults to `stderr` and continues.

| Key | Type | Default | Effect |
|---|---|---|---|
| `max_constraints_per_micro_it` | int | `std::numeric_limits<int>::max()` (no limit) | Maximum number of **new** constraint families injected into a subproblem per micro iteration. Families already added do not count. |
| `add_N_constraint` | bool | `false` | When true, a branch that already got an N constraint in the same call is not additionally constrained under any N-K scenario. |
| `tol_N` | double | `1.001` | N flow-limit tolerance multiplier. A branch is overloaded only if its flow exceeds `tol_N * max_flow_N`. |
| `tol_N_K` | double | `1.0` | N-K flow-limit tolerance multiplier. A branch is overloaded only if its flow exceeds `tol_N_K * max_flow_N_K`. |

Example:

```json
{
  "max_constraints_per_micro_it": 150,
  "add_N_constraint": true,
  "tol_N": 1.005,
  "tol_N_K": 1.0
}
```

Note the JSON key is `add_N_constraint`, but internally the member is named `add_N_constraint_first_` and the log line prints `add_N_constraint_first`. These refer to the same knob. See [Constraint selection](grid-modelisation/constraint-selection.md).

## Input files

All paths are relative to the directory the process runs from.

| Path | Consumed by | Purpose | Detailed in |
|---|---|---|---|
| `plugin_inputs/plugin_params.json` | `Plugin::load_config` | Tuning knobs | This page |
| `plugin_inputs/inputs_micro_it/B_inv` (`.bin` + `_rownames.json` + `_colnames.json`) | `Plugin` constructor | Inverse nodal susceptance matrix | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/inputs_micro_it/Ab` (`.coo` + names) | `Plugin` constructor | Branch-node incidence matrix | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/inputs_micro_it/Yl` (`.coo` + names) | `Plugin` constructor | Line admittance matrix | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/inputs_micro_it/A_hvdc` (`.coo` + names) | `Plugin` constructor | HVDC injection-node mapping | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/inputs_micro_it/branches_to_candidates_dict.json` | `Plugin` constructor | Branch name -> candidate name | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/inputs_micro_it/n_side1_dict.json`, `n_side2_dict.json` | `Plugin` constructor | Branch name -> side-1/side-2 node number | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/inputs_micro_it/max_flows_N.json`, `max_flows_N_K.json` | `Plugin` constructor | (branch, scenario) -> flow limit | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/inputs_micro_it/dict_incident_outage_AC_branches.json`, `dict_incident_HVDC_branches.json` | `Plugin` constructor | Incident id -> outaged branches | [Data loading](grid-modelisation/data-loading.md) |
| `plugin_inputs/constraints_dictionary.csv` | `read_constraints_dict` | Family name -> constraint identifiers | [C++ plugin boundary](cpp-plugin.md) |
| `plugin_inputs/variables_dictionary.csv` | `read_variables_dictionary` | Solver variable name -> internal flow key | [C++ plugin boundary](cpp-plugin.md) |
| `plugin_inputs/investment_dictionary.csv` | `read_binary_variables_ids_map` | Master variable name -> branch id | [C++ plugin boundary](cpp-plugin.md) |

The `benchmark/update_configs.py` script writes `plugin_params.json` and sets `OUTPUTROOT` in the study's `options.json`; `benchmark/run_benchmark.sh` then `cd`s into the study directory and launches the Benders executable with `mpirun`, which is why the relative `./plugin_inputs/...` paths resolve correctly.
