# Grid modelisation

**Summary:** The `Plugin` class in `gridModelisation.h` holds all the electrical-grid modeling. It loads pre-computed grid data, recomputes flow sensitivities (PTDF, HVDC, N-K incident factors) after candidate lines are removed, detects flow-limit violations, and selects which constraint families to inject during a micro-iteration.
**Main code:** `cpp_plugin/gridModelisation.h`
**Parent:** [Entry](../entry.md)
**Related:** [C++ plugin boundary](../cpp-plugin.md), [Configuration](../configuration.md), [Glossary](../glossary.md)

## Purpose

The Benders framework knows nothing about the electrical grid; it only produces investment decisions and subproblem solutions. The `Plugin` is the piece that interprets those decisions as a change in grid topology and turns a subproblem solution into a set of violated flow constraints. Everything electrical lives in this one header.

## What the `Plugin` holds

The constructor loads these members from the data directory (see [Data loading](data-loading.md)):

| Member | Type | Meaning |
|---|---|---|
| `B_inv_` | dense `NamedMatrix` (int keys) | Inverse nodal susceptance matrix; keys are node numbers |
| `Ab_` | sparse `NamedMatrix` (string keys) | Branch-node incidence matrix |
| `Yl_` | sparse `NamedMatrix` (string keys) | Line admittance matrix |
| `A_hvdc_` | sparse `NamedMatrix` (string keys) | HVDC injection-node mapping |
| `branches_dict_` | `map<string,string>` | Branch name -> candidate name |
| `n_side1_dict_`, `n_side2_dict_` | `map<string,int>` | Branch name -> side-1/side-2 node number |
| `max_flows_N_`, `max_flows_N_K_` | `map<pair<string,string>,double>` | (branch, scenario) -> flow limit |
| `dict_incident_outage_AC_branches_` | `map<string,vector<string>>` | Incident id -> outaged AC branches |
| `dict_incident_HVDC_branches_` | `map<string,vector<string>>` | Incident id -> outaged HVDC branches |

Computed at runtime and shared across ranks:

| Member | Type | Meaning |
|---|---|---|
| `all_monitored_branches_` | `vector<string>` | Monitored branches after removing non-invested ones |
| `HVDC_new_dict_` | `map<pair<string,string>,double>` | (HVDC, monitored branch) -> sensitivity |
| `dict_incident_factors_` | `map<tuple<string,string,string>,double>` | (monitored, incident, outaged AC branch) -> factor |

Configuration fields (`max_constraints_per_micro_it_`, `add_N_constraint_first_`, `tol_N_`, `tol_N_K_`) are described in [Configuration](../configuration.md) and [Constraint selection](constraint-selection.md).

## Data flow

```text
Plugin(data_path, world, config_path)                data-loading.md
  -> load_config + load all grid data

compute_factors_for_micro_iterations(z_dict)         sensitivity-computation.md
  -> get_invested_branches
  -> compute_sensis_after_lines_removal
       -> update_PTDF_after_lines_removal (Woodbury)
       -> update_HVDC_sensi_after_lines_removal
  -> compute_incident_factors_after_lines_removal
  -> populate HVDC_new_dict_, dict_incident_factors_, all_monitored_branches_

broadcast_factors()                                  sensitivity-computation.md
  -> MPI broadcast the three computed members

return_constraints_for_micro_iteration(v, F_N)       constraint-selection.md
  -> get_overflows_N
  -> get_overflows_N_K
  -> sort_results_and_return_constraints
```

## Pages in this folder

| Page | What it covers |
|---|---|
| [Data loading](data-loading.md) | `NamedMatrix`, the `load_*` readers, binary/JSON formats, index conventions, the constructor |
| [Sensitivity computation](sensitivity-computation.md) | PTDF via Woodbury, HVDC sensitivities, incident factors, `compute_factors_for_micro_iterations`, `broadcast_factors` |
| [Constraint selection](constraint-selection.md) | N and N-K overflow detection, sorting, the constraint budget, `return_constraints_for_micro_iteration` |
