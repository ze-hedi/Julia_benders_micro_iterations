# C++ plugin boundary

**Summary:** `micro_iters_cpp_plugin.cpp` is the `extern "C"` boundary between the Antares-xpansion Benders framework and the grid-modeling `Plugin`. It reads three CSV dictionaries, holds the `Plugin` and logger as singletons, tracks which constraint families were already added per subproblem, and implements the `OnBenders*` callbacks.
**Main code:** `cpp_plugin/micro_iters_cpp_plugin.cpp`
**Parent:** [Entry](entry.md)
**Related:** [Grid modelisation](grid-modelisation/index.md), [Logging](logging.md), [Configuration](configuration.md)

## Purpose

This file contains no grid math and no I/O logic of its own. It exists to translate between the framework's callback arguments and the `Plugin` class. Its three jobs are: (1) load and cache the CSV dictionaries, (2) hold long-lived singletons, and (3) perform the callback work — building `z_dict` from the master solution, building `F_N_values` from the subproblem solution, and deduplicating the violated constraint families before returning them.

## Free functions and singletons

Most dictionaries are cached in function-local `static` objects, so the path argument is only honored on the **first** call; later calls return the same data.

| Function | Location | Role |
|---|---|---|
| `read_constraints_dict` | `micro_iters_cpp_plugin.cpp` | Parse `constraints_dictionary.csv` into `map<family, vector<constraint id>>` |
| `get_constraints_dict` | `micro_iters_cpp_plugin.cpp` | Singleton accessor (returns by value) |
| `read_variables_dictionary` | `micro_iters_cpp_plugin.cpp` | Parse `variables_dictionary.csv` into `map<solver name, flow key>` |
| `get_plugin` | `micro_iters_cpp_plugin.cpp` | Singleton `shared_ptr<Plugin>`, built once with hard-coded paths |
| `micro_iterations_logger` | `micro_iters_cpp_plugin.cpp` | Namespace-scope `shared_ptr<MicroIterationsLog>` |
| `get_variables_dictionary` | `micro_iters_cpp_plugin.cpp` | Singleton accessor (returns const ref) |
| `read_binary_variables_ids_map` | `micro_iters_cpp_plugin.cpp` | Parse `investment_dictionary.csv` into `map<master var name, branch id>` |
| `get_added_constraints_families_per_sub` | `micro_iters_cpp_plugin.cpp` | Singleton map: subproblem -> added family names |
| `get_added_constraints_families_per_sub_is_N` | `micro_iters_cpp_plugin.cpp` | Singleton map: subproblem -> parallel `bool` (N=true / N-K=false) |
| `clean_added_constraints_families_per_sub` | `micro_iters_cpp_plugin.cpp` | Clear both tracking vectors (keeps keys) |
| `get_binary_variables_ids_map` | `micro_iters_cpp_plugin.cpp` | Singleton accessor for the investment dictionary |
| `check_if_constraints_family_added` | `micro_iters_cpp_plugin.cpp` | Linear search: was this family already added to this sub? |
| `get_master_resolution_start` | `micro_iters_cpp_plugin.cpp` | Singleton `steady_clock` time point (defined but unused) |

`get_plugin` constructs the `Plugin` with the hard-coded relative paths `./plugin_inputs/inputs_micro_it` and `./plugin_inputs/plugin_params.json`, plus the MPI communicator pointer. The `world` argument only takes effect on the first call.

## CSV dictionaries

All three files live under `<root>/plugin_inputs/`. Parsing uses Boost's `escaped_list_separator` tokenizer; the parsers do **not** skip a header line, so a header row would be ingested as data.

| File | Read by | Row layout (positional) | Produced map |
|---|---|---|---|
| `constraints_dictionary.csv` | `read_constraints_dict` | col 0 = family name; cols 1..N = constraint ids | `map<family_name, vector<constraint_id>>` |
| `variables_dictionary.csv` | `read_variables_dictionary` | col 1 = solver variable name; col 0 = internal flow key | `map<solver_name, flow_key>` |
| `investment_dictionary.csv` | `read_binary_variables_ids_map` | col 1 = master variable name; col 0 = branch id | `map<master_var_name, branch_id>` |

The column positions are confirmed from the parsing code; the semantic labels ("solver name", "branch id", etc.) are inferred from how each map is consumed in the callbacks. A missing CSV is fatal: the reader prints to `stderr` and calls `exit(EXIT_FAILURE)`.

## Tracking maps

`get_added_constraints_families_per_sub` and `get_added_constraints_families_per_sub_is_N` form a pair keyed by subproblem name. The first stores the **names** of constraint families already injected into that subproblem; the second stores a parallel `bool` for each, where `true` means an N constraint and `false` means an N-K constraint. They are always pushed and cleared together, so index `i` of one corresponds to index `i` of the other. They are reset by `clean_added_constraints_families_per_sub` at the start of each Benders iteration.

## Callbacks

### OnBendersStart

```cpp
void OnBendersStart(std::filesystem::path input_root, std::filesystem::path output_root,
                    bool warm_start, mpi::communicator* world, int log_level)
```

Loads the variables dictionary (from `output_root`), the constraints dictionary (from `input_root`), builds the `Plugin` singleton, constructs the logger, and forces initialization of the tracking maps. The returned dictionaries are stored in locals only to trigger the static initialization.

Note an asymmetry: the variables dictionary is first read with `output_root` here, but later `OnBendersMicroIterationEnd` asks for `input_root`. Because the singleton is initialized on first use, the `output_root` version wins. See [Open questions](#open-questions).

### OnBendersIterationStart

Calls `clean_added_constraints_families_per_sub`, clearing the per-subproblem "already added" records for a fresh Benders iteration.

### OnBendersMicroIterationEnd

```cpp
void OnBendersMicroIterationEnd(std::string sub_name, std::string solving_time,
                                std::vector<double> sub_solution,
                                std::vector<int>& variables_indices_vector,
                                std::vector<std::string>& variables_names_vector,
                                std::filesystem::path input_root,
                                std::vector<std::string>& constraints_to_add_vec,
                                int num_master_iter, int num_micro_iter,
                                int n_saturated_constraints, int n_all_constraints)
```

Steps:

1. Build `F_N_values` (a `map<flow_key, double>`): for each index `i`, look up the solver variable name `variables_names_vector[i]` in the variables dictionary to get the internal flow key, and read the value `sub_solution[variables_indices_vector[i]]`. The two vectors are parallel arrays.
2. Call `plugin->return_constraints_for_micro_iteration(sub_name, F_N_values, already_added)` to get `vector<pair<family_name, is_N>>`. The third argument is the list of families already added to this subproblem, so they do not count against the per-iteration budget.
3. For each returned family, skip it if `check_if_constraints_family_added` finds it already present. Otherwise append the family name and its `is_N` flag to the two tracking maps, and append the family's constraint identifiers (`constraints_dict[family_name]`) to `constraints_to_add_vec`.
4. Log with `DumpAddedConstraints` and `AddMicroIterionLog`.

The deduplication in step 3 uses `constraints_dict[family_name]` with `operator[]` (not `.at()`), so a family missing from the dictionary silently contributes nothing instead of throwing.

### OnBendersMasterResolutionEnd

```cpp
void OnBendersMasterResolutionEnd(std::map<std::string, double>& master_out,
                                  double master_resolution_time,
                                  int& num_iter,
                                  mpi::communicator* world,
                                  std::filesystem::path input_root)
```

1. On rank 0 only: fetch the investment dictionary and build `z_dict` (a `map<branch_id, int>`). For each `(var_name, var_id)` in the dictionary, if `master_out` contains `var_name`, set `z_dict[var_id] = static_cast<int>(master_out[var_name])`. Investment variables absent from `master_out` are skipped (no entry).
2. On rank 0: call `plugin->compute_factors_for_micro_iterations(z_dict)` and time it.
3. Broadcast the elapsed time to all ranks, log the master iteration, and call `plugin->broadcast_factors()` on all ranks to distribute the recomputed factors.

The `num_iter` parameter is passed by reference but only read.

### Stub callbacks

`OnBendersEnd`, `OnBendersIterationEnd`, `OnBendersMicroIterationStart`, `OnBendersMasterResolutionStart`, `OnBendersSubResolutionStart`, and `OnBendersSubResolutionEnd` are empty. In particular, `OnBendersMasterResolutionStart` is empty even though `get_master_resolution_start` exists, so that timer is dead code; the only master timing measured is the factor recomputation inside `OnBendersMasterResolutionEnd`.

## Edge cases and unusual behavior

- **First-path-wins on singletons:** because the dictionary getters use function-local `static`, only the first path argument matters for the whole run.
- **No header-row skipping:** a header in any CSV becomes a bogus map entry.
- **Silent missing family:** `constraints_dict[family_name]` on an unknown family appends nothing with no diagnostic.
- **`check_if_constraints_family_added` auto-inserts** an empty entry for an unseen subproblem via `operator[]`.
- **`z_dict` is partial:** master variables not present in `master_out` produce no `z_dict` entry at all.

## Open questions

- Whether the runtime CSV files contain header rows cannot be confirmed: they are not committed to the repository.
- Whether the `output_root` vs `input_root` asymmetry for `variables_dictionary.csv` is intentional (only `output_root` takes effect due to static init).
- Why `get_master_resolution_start` exists but is never called (likely vestigial from an earlier design).
