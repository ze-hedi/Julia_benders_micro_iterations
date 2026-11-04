# Sensitivity computation

**Summary:** After the master picks candidate lines, the `Plugin` removes the non-invested branches from the grid model and recomputes three things: the PTDF matrix (via a Woodbury update of `B_inv`), the HVDC sensitivity matrix, and the N-K incident factors. These are computed once per master iteration on rank 0 and broadcast to all ranks.
**Main code:** `cpp_plugin/gridModelisation.h`
**Parent:** [Grid modelisation](index.md)
**Related:** [Data loading](data-loading.md), [Constraint selection](constraint-selection.md), [Glossary](../glossary.md)

## Purpose

A master solution changes which candidate lines are built, which changes the grid topology. The flow limits and the outaged-branch lists are fixed, but the sensitivities that map injections and outages to line flows depend on the topology. This page explains how the plugin recomputes those sensitivities efficiently, without re-factorizing the whole grid.

## Step 1 — split branches by investment decision

`get_invested_branches(z_dict, invested_branches, non_invested_branches)` iterates `branches_dict_` (branch -> candidate). For each branch, it reads `z_dict.at(candidate)`; a value of exactly `1` puts the branch in `invested_branches`, anything else puts it in `non_invested_branches`. The non-invested branches are the lines "removed" from the topology for the rest of the computation.

## Step 2 — recompute the PTDF

`update_PTDF_after_lines_removal(non_invested_branches)` performs a low-rank Woodbury update of `B_inv` and rebuilds the PTDF matrix.

### Build `A_k` (the removed-line susceptance)

For each removed branch `br`, it takes the incidence row `a = Ab_.mat.row(Ab_.row_index.at(br))` and the diagonal admittance `yl = Yl_.mat.coeff(Yl_.row_index.at(br), Yl_.col_index.at(br))`, then sets column `j` of `A_k` to `sqrt(yl) * a`. `A_k` is `n x k` where `n` is the number of nodes and `k` the number of removed branches.

### Woodbury update

```text
BinvA   = B_inv * A_k
M       = I - A_k^T * BinvA
M_inv   = M^-1
Bnew_inv = B_inv + BinvA * M_inv * BinvA^T
```

This is the Sherman-Morrison-Woodbury identity for `B_new = B - A_k A_k^T`, i.e. removing the susceptance contributed by the removed lines. `Bnew_inv` inherits `B_inv_`'s row and column index maps (node numbers -> 0-based columns).

### Remove lines from `Ab` and `Yl`

The function then drops the removed branches from `Ab` (keeping the kept rows) and from `Yl` (keeping the kept rows and columns). `Ab_new` keeps the same node columns; `Yl_new` becomes `remaining x remaining`.

### Compute the PTDF

```text
PTDF_new.mat = Yl_new.mat * Ab_new * Bnew_inv.mat
PTDF_new.row_index = Yl_new.row_index   (branch names)
PTDF_new.col_index = Bnew_inv.col_index (node numbers)
```

`PTDF = Yl * Ab * B_inv` is the standard DC power-flow identity: nodal injections `P` map to nodal angles `theta = B^-1 P`, and a line flow is its admittance times the angle difference, `Yl * Ab * theta`. Rows are lines, columns are nodes.

## Step 3 — HVDC sensitivities

`update_HVDC_sensi_after_lines_removal(PTDF_new)` computes `HVDC_sensitivity_matrix_new.mat = PTDF_new.mat * A_hvdc_.mat`, with column index from `A_hvdc_` and row index from `PTDF_new`. `A_hvdc_` is nodes x HVDC, so the product is lines x HVDC: how much each HVDC injection changes each line flow.

`compute_sensis_after_lines_removal` wraps steps 2 and 3, then thresholds both matrices at `1e-5`: entries with `abs(value) < 1e-5` become exactly `0`.

## Step 4 — incident factors (post-contingency)

`compute_incident_factors_after_lines_removal(all_monitored_branches, branches_invested, PTDF_new)` computes the post-contingency factors. (The `branches_invested` parameter is received but never used.)

For each incident `incident_id` with outaged AC branches:

1. Build a PTDF lookup `PTDF_new_dict[(node, branch)] = value` for monitored branches, keeping only non-zero entries.
2. For each outaged branch, look up its side-1 and side-2 node columns via `PTDF_new.col_index.at(n_side1_dict_.at(br))` and `.at(n_side2_dict_.at(br))`, and its row via `PTDF_new.row_index.at(br)`. Here `n_side1_dict_`/`n_side2_dict_` return a **node number**, which is the key into `PTDF_new.col_index` (see [Data loading](data-loading.md)).
3. Build `nb_outage x nb_outage` matrices `PTDF_side1(i,j) = PTDF_new.mat(outage_rows[i], side1_cols[j])` and `PTDF_side2(i,j) = PTDF_new.mat(outage_rows[i], side2_cols[j])`.
4. Compute the factor matrix `FPM = (I - (PTDF_side1 - PTDF_side2))^-1`.
5. For each monitored line and each outaged line, sum over all outaged lines `outage_line_bis`:

```text
incident_factor += FPM(outage_idx[outage_line_bis], outage_idx[outage_line])
                   * (PTDF(monitored, node1(outage_line_bis)) - PTDF(monitored, node2(outage_line_bis)))
```

The result is stored as `dict_incident_factors[(monitored_line, incident_id, outage_line)]`. Finally, entries with `abs(value) < 1e-5` are zeroed (keys kept).

## Step 5 — top-level orchestration

`compute_factors_for_micro_iterations(z_dict)` ties it together:

1. Split branches with `get_invested_branches`.
2. Build `all_monitored_branches_` as the union of branch names in `max_flows_N_` and `max_flows_N_K_`, minus the non-invested branches.
3. Collect `branches_with_incidents` (all branches named in any incident's outage list).
4. Call `compute_sensis_after_lines_removal` to get `PTDF_new` and the HVDC matrix.
5. Keep HVDC rows whose name is in `all_monitored_branches_` or `branches_with_incidents`, and convert the filtered HVDC matrix to `HVDC_new_dict_` (keyed `(HVDC, monitored_branch)`) via `helper_convert_sensitivity_array_to_dict`.
6. Compute `dict_incident_factors_` from `PTDF_new` and `all_monitored_branches_`.

`helper_convert_sensitivity_array_to_dict` builds keys as `(col_key, row_key)` — column first — so for the HVDC matrix the key is `(HVDC_name, monitored_branch)`, matching the lookup used in [Constraint selection](constraint-selection.md).

## Step 6 — MPI broadcast

`broadcast_factors` broadcasts three members from rank 0: `dict_incident_factors_`, `HVDC_new_dict_`, and `all_monitored_branches_`. This is why the header includes Boost serialization for `map`, `vector`, `string`, and `utility`, and defines a `serialize` overload for `std::tuple` (the key type of `dict_incident_factors_`).

## Edge cases and unusual behavior

- `Yl_new.col_index` is never populated in `update_PTDF_after_lines_removal` (only `row_index` is). It is unused downstream, but it is a latent trap if the matrix is reused.
- The `branches_invested` parameter of `compute_incident_factors_after_lines_removal` is unused.
- All `.at()` lookups throw `std::out_of_range` if a branch or node name is missing from the loaded maps.
- `PTDF_new` thresholds at `1e-5` zero out small values but keep the matrix dimensions and names.
