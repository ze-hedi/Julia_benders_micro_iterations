# Data loading

**Summary:** The `Plugin` constructor reads a fixed set of files from the data directory using the `NamedMatrix` wrapper and several `load_*` helper functions. This page describes those helpers, the binary and JSON file formats, and the Julia-to-C++ index conventions.
**Main code:** `cpp_plugin/gridModelisation.h`
**Parent:** [Grid modelisation](index.md)
**Related:** [Configuration](../configuration.md), [Sensitivity computation](sensitivity-computation.md)

## Purpose

The grid data (matrices and dictionaries) is pre-computed on the Julia side and dumped to disk. The plugin must load it exactly as Julia wrote it, including translating Julia's 1-based indexing to C++'s 0-based indexing where an *index value* is involved. Getting this convention right is the main correctness risk when extending the loader or regenerating the data.

## `NamedMatrix`

```cpp
template <typename MatrixType, typename RowKey = std::string, typename ColKey = std::string>
struct NamedMatrix {
    MatrixType mat;
    std::map<RowKey, int> row_index;
    std::map<ColKey, int> col_index;
};
```

It bundles an Eigen matrix with two name-to-index maps. `mat` is a dense `MatrixXd` or a `SparseMatrix<double>`. `row_index`/`col_index` map a name (a branch name, a node number, or an HVDC name) to a 0-based matrix row/column. This is what lets the code address rows by branch name and columns by node number.

## Loaders

| Loader | Input | Output | Index handling |
|---|---|---|---|
| `load_pair_keyed_dict` | JSON with keys like `("a", "b")` | `map<pair<string,string>,double>` | none (strings) |
| `load_dict_of_vectors` | JSON object of arrays | `map<string,vector<string>>` | none |
| `load_dict` | generic JSON object | `map<key,value>` | none (values verbatim) |
| `load_names` | JSON object of `{name: index}` | `map<Key,int>` | **value - 1** (Julia 1-based -> C++ 0-based) |
| `load_dense_named` | binary `.bin` + `_rownames.json`/`_colnames.json` | dense `NamedMatrix` | names via `load_names` |
| `load_sparse_named` | binary `.coo` + names JSON | sparse `NamedMatrix` | names via `load_names`; COO coordinates verbatim |
| `load_branches_from_txt` | plain text lines | `vector<string>` | none (unused in this header) |

### `load_names`

For a JSON object `{name: index}`, it converts the key to `Key` (using `std::stoi` when `Key` is `int`) and stores the value as `index - 1`. This is the Julia 1-based -> C++ 0-based conversion.

### `load_dense_named`

Reads `prefix.bin` as: two `int64_t` values `rows`, `cols`, then `rows * cols` `double`s. Julia is column-major and Eigen defaults to column-major, so the blob is copied directly. Row and column names come from `prefix_rownames.json` and `prefix_colnames.json` via `load_names`.

### `load_sparse_named`

Reads `prefix.coo` as: three `int64_t` headers `rows`, `cols`, `nnz`, then three arrays of length `nnz`: `I` (int64 row indices), `J` (int64 column indices), `V` (double values). It builds `Eigen::Triplet` objects and calls `setFromTriplets`. The COO coordinates `I`/`J` are used **as-is** with no `-1` offset (they are already 0-based in the committed test data). Names come from the JSON via `load_names`.

## Binary / JSON file formats

The constructor loads these files from `data_path`:

| File | Loader | Result member |
|---|---|---|
| `B_inv.bin` + `B_inv_rownames.json` + `B_inv_colnames.json` | `load_dense_named` | `B_inv_` (dense, int keys) |
| `Ab.coo` + `Ab_rownames.json` + `Ab_colnames.json` | `load_sparse_named<string,string>` | `Ab_` |
| `Yl.coo` + names | `load_sparse_named<string,string>` | `Yl_` |
| `A_hvdc.coo` + names | `load_sparse_named<string,string>` | `A_hvdc_` |
| `branches_to_candidates_dict.json` | `load_dict` | `branches_dict_` |
| `n_side1_dict.json`, `n_side2_dict.json` | `load_dict<string,int>` | `n_side1_dict_`, `n_side2_dict_` |
| `max_flows_N.json`, `max_flows_N_K.json` | `load_pair_keyed_dict` | `max_flows_N_`, `max_flows_N_K_` |
| `dict_incident_outage_AC_branches.json` | `load_dict_of_vectors` | `dict_incident_outage_AC_branches_` |
| `dict_incident_HVDC_branches.json` | `load_dict_of_vectors` | `dict_incident_HVDC_branches_` |

## Index conventions in the committed test data

`tests/unit_tests/test_utils/cpp_structures/` shows the real conventions:

- `B_inv_colnames.json` is `{"2":1, "3":2, ...}`: the keys are node numbers (node 1 is the reference/slack node and is absent), and the values are 1-based column indices. After `load_names<int>` this becomes `{2:0, 3:1, ...}` — keys are node numbers, values are 0-based columns.
- `Ab_rownames.json` is `{"LINE-101-102":1, ...}`: branch name -> 1-based row index, converted to 0-based.
- `n_side1_dict.json` / `n_side2_dict.json` are `{"LINE-103-109":3, ...}`: branch name -> **node number**. These are loaded with `load_dict<string,int>` (no `-1`), which is correct because they are used as **keys** into `PTDF_new.col_index` (which is keyed by node number), not as matrix indices.

So the rule is: the `-1` conversion applies only to *index values* stored in the name maps; node numbers and branch names are identifiers and keep their Julia numbering. See the node-number lookup in [Sensitivity computation](sensitivity-computation.md).

## Edge cases and unusual behavior

- `load_pair_keyed_dict` parses keys by finding the first four `"` characters, so it breaks if a branch or scenario name contains a quote character.
- `load_names` only applies `-1` when `Key` is the map's *value* type; `load_dict<string,int>` does not decrement, so the two must not be mixed for the same kind of value.
- `load_branches_from_txt` is defined but not called anywhere in this header.
- Missing files are not handled uniformly: the JSON readers use `json::parse` and throw on a missing/invalid file; `load_config` is the only loader that checks for an open failure and falls back to defaults.
