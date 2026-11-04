# Constraint selection

**Summary:** Given a subproblem's solved flows, the `Plugin` detects which branches exceed their N and N-K flow limits, sorts the violations by size, and returns the worst constraint families to inject, subject to a per-micro-iteration budget of new families.
**Main code:** `cpp_plugin/gridModelisation.h`
**Parent:** [Grid modelisation](index.md)
**Related:** [Sensitivity computation](sensitivity-computation.md), [Configuration](../configuration.md), [C++ plugin boundary](../cpp-plugin.md)

## Purpose

The micro-iteration loop starts each subproblem with a small constraint set and adds the violated constraints one batch at a time. This page explains how the plugin decides *which* constraints are violated and *which order* to add them, while keeping the subproblem from growing too fast.

## Detecting N violations

`get_overflows_N(max_flows_N, all_monitored_branches, v, F_N_values, tol_N)`:

For each monitored branch, it compares `abs(F_N_values.at(branch))` to `abs(max_flows_N.at({branch, v})) * tol_N`. If the flow exceeds the tolerance-scaled limit, it records the overflow as `flow - max_flow` (positive). Returns `map<branch, overflow>`.

Here `v` is the subproblem (scenario) name, the second key of the `max_flows_N` map.

## Detecting N-K violations

`get_overflows_N_K(...)` iterates over each incident. For each monitored branch (skipping branches that are themselves outaged in that incident), it builds the post-contingency flow:

```text
F_N_K = F_N_values[monitored]
      + sum over outaged AC branches: F_N_values[outage] * dict_incident_factors[{monitored, incident, outage}]
      - sum over outaged HVDC:        F_N_values[outage_hvdc] * HVDC_new_dict[{outage_hvdc, monitored}]
```

If `abs(F_N_K) > abs(max_flows_N_K.at({monitored, v})) * tol_N_K`, it records `abs(F_N_K) - max_flow` under the key `{monitored, incident}`. Returns `map<pair<branch, incident>, overflow>`.

## Selecting the constraint families

`sort_results_and_return_constraints(dict_results_overflow_N, dict_results_overflow_N_K, already_added)` returns a `vector<pair<string, bool>>` where `true` means an N family and `false` means an N-K family.

It sorts both overflow maps **descending by overflow value**, then:

**N constraints** (family `branch_<monitored>`):
- If the family is new and adding it would exceed `max_constraints_per_micro_it_`, `break` (stop the whole N loop).
- Otherwise push `{family, true}`. A family already in `already_added` is re-emitted but does not consume budget.

**N-K constraints** (families `branch_<monitored>_inc_<incident>` and `inc_<incident>`):
- Skip a branch if it already got an N-K constraint in this call (`already_in_N_K`).
- Skip a branch if `add_N_constraint_first_` is true and the branch already got an N constraint in this call (the N constraint is deemed to cover it).
- Compute `cost` = 1 for each of the two families that is not already known (so up to 2 new families).
- If `n_new_families + cost > max_constraints_per_micro_it_`, `break` (stop the whole N-K loop).
- Otherwise push both families with `false`, mark the branch as N-K-constrained, and add `cost` to the new-family count.

### Why `break` instead of `skip`

The budget check uses `break`, not `continue`, so a lower-priority (smaller) overflow can never fill the remaining budget ahead of a larger overflow that did not fit. The `continue` cases are only for genuine deduplication/coverage, not for budget.

## Public entry point

`return_constraints_for_micro_iteration(sub_problem, F_N_values, already_added = {})`:

1. Calls `get_overflows_N` and `get_overflows_N_K` with the member data and the configured tolerances.
2. Calls `sort_results_and_return_constraints` with `already_added` (families already injected into this subproblem; they do not count against the budget).
3. Returns the selected families.

The caller in [C++ plugin boundary](../cpp-plugin.md) then deduplicates against its own per-subproblem tracking and maps each family name to its constraint identifiers.

## Configuration

| Key | Default | Read at | Effect |
|---|---|---|---|
| `max_constraints_per_micro_it` | no limit | `load_config` | Max **new** families per micro iteration |
| `add_N_constraint` | `false` | `load_config` | N constraint suppresses N-K for the same branch |
| `tol_N` | `1.001` | `load_config` | N violation multiplier |
| `tol_N_K` | `1.0` | `load_config` | N-K violation multiplier |

`load_config` reads `plugin_params.json`; if it cannot open the file it prints the defaults and continues. If the file exists but contains invalid JSON, `json::parse` throws (no try/catch). `print_config` writes the effective values to `stderr`. See [Configuration](../configuration.md).

## Edge cases and unusual behavior

- The reported overflow is always the unscaled excess `flow - max_flow` (N) or `abs(flow) - max_flow` (N-K); the tolerance only decides *whether* a branch is violated.
- N-K output can repeat a family name already present in `already_added`; only the budget accounting deduplicates.
- One N-K overflow can emit two families (`branch_..._inc_...` and `inc_...`), both tagged `is_N == false`.
- Missing keys in `max_flows_*` or `F_N_values` throw `std::out_of_range`.
