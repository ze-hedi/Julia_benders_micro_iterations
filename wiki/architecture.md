# Architecture

**Summary:** The repository builds a single shared library, `cpp_plugin.so`, that Antares-xpansion loads at runtime to add grid-modeling logic to its Benders decomposition. All grid math lives in `gridModelisation.h`; the callback boundary lives in `micro_iters_cpp_plugin.cpp`; logging support lives under `utils/`.
**Main code:** `CMakeLists.txt`, `cpp_plugin/CMakeLists.txt`, `utils/CMakeLists.txt`
**Parent:** [Entry](entry.md)
**Related:** [C++ plugin boundary](cpp-plugin.md), [Grid modelisation](grid-modelisation/index.md), [Logging](logging.md)

## Purpose

Antares-xpansion uses Benders decomposition to find optimal candidate transmission lines. The framework itself is kept decoupled from the electrical-grid model: it exposes lifecycle callbacks and loads an external plugin at runtime. This project provides that plugin. It is responsible for two things the framework does not know about: (1) recomputing flow sensitivities when a master solution changes the grid topology, and (2) detecting and selecting the violated flow constraints to inject during micro-iterations.

## Components

The build produces one artifact, the shared library `cpp_plugin.so`.

```text
CMakeLists.txt          top-level build: C++20, MPI, Boost (mpi+serialization), Eigen3, nlohmann_json
├── cpp_plugin/         the plugin -> cpp_plugin.so
│   ├── gridModelisation.h              all grid modeling + math (Plugin class)
│   └── micro_iters_cpp_plugin.cpp      extern "C" OnBenders* callbacks (plumbing)
├── utils/              INTERFACE library "utils" shared by the plugin and tests
│   ├── micro_iterations_logger.{h,cpp} async file logging
│   └── micro_iterations_http_sender.{h,cpp} raw-socket JSON POST client
└── tests/              GoogleTest unit tests (built only with -DBUILD_TESTS=ON)
    └── unit_tests/     test_plugin.cpp, test_micro_iterations_logger.cpp
```

Outside the build: `parse_micro_iterations.py` parses the log files, and `benchmark/` has a helper to set plugin parameters and run a study. The committed grid test data lives in `tests/unit_tests/test_utils/cpp_structures/`.

## Build

The top-level `CMakeLists.txt` sets C++20 with `-O3` and requires MPI, Boost 1.66 (`mpi` and `serialization`), Eigen3, and nlohmann_json. `utils` is an INTERFACE library; `cpp_plugin` is a SHARED library named `cpp_plugin.so` (empty `PREFIX`, `SUFFIX ".so"`). Tests are opt-in via `-DBUILD_TESTS=ON`, which fetches GoogleTest v1.15.2 with CMake `FetchContent`.

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

Two GitHub Actions workflows build and package the artifact: `ci-ubuntu.yml` (Ubuntu 22.04) and `ci-ol8.yml` (Oracle Linux 8 with manually built OpenMPI 4.1.8 and Boost 1.66).

## Callback surface

The Benders framework calls these `extern "C"` functions. Only four do work; the rest are empty stubs.

| Callback | Purpose | Where implemented |
|---|---|---|
| `OnBendersStart` | Load dictionaries, build `Plugin` and logger | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersEnd` | (stub) | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersIterationStart` | Reset per-subproblem "already added" tracking | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersIterationEnd` | (stub) | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersMicroIterationStart` | (stub) | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersMicroIterationEnd` | Detect violated constraints, fill `constraints_to_add_vec` | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersMasterResolutionStart` | (stub) | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersMasterResolutionEnd` | Build `z_dict`, recompute factors on rank 0, broadcast | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersSubResolutionStart` | (stub) | [C++ plugin boundary](cpp-plugin.md) |
| `OnBendersSubResolutionEnd` | (stub) | [C++ plugin boundary](cpp-plugin.md) |

## Main execution path

```text
OnBendersStart (micro_iters_cpp_plugin.cpp)
  -> get_variables_dictionary / get_constraints_dict / get_plugin / logger
OnBendersMasterResolutionEnd (micro_iters_cpp_plugin.cpp)
  -> [rank 0] build z_dict from master_out
  -> Plugin::compute_factors_for_micro_iterations (gridModelisation.h)
  -> MPI broadcast factors
OnBendersMicroIterationEnd (micro_iters_cpp_plugin.cpp)
  -> build F_N_values from sub_solution
  -> Plugin::return_constraints_for_micro_iteration (gridModelisation.h)
  -> dedup + append to constraints_to_add_vec
```

The plugin resolves its input paths relative to the working directory: `./plugin_inputs/inputs_micro_it` for grid data and `./plugin_inputs/plugin_params.json` for configuration (see [Configuration](configuration.md)). This is why the benchmark script `cd`s into the study directory before launching the Benders executable.
