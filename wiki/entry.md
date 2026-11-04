# Julia Benders micro iterations wiki

**Commit:** c5377a3
**Last updated:** 2026-11-04

## Overview

This repository builds `cpp_plugin.so`, a shared library that the Antares-xpansion Benders-decomposition framework loads at runtime through dynamic linking. It implements the **micro-iteration** refinement step for electrical grid investment problems.

The Benders master problem selects candidate transmission lines. Each new set of invested lines changes the grid topology, so the plugin recomputes the grid's flow sensitivities (PTDF, HVDC sensitivities, and N-K incident factors). The subproblems are then solved starting from a small set of constraints; after each solve, the plugin checks which flow-limit constraints are violated, injects the worst violated ones, and the subproblem is re-solved. This repeats until no violated constraint remains.

The two files that do the real work are [`gridModelisation.h`](grid-modelisation/index.md) (all grid modeling and math) and [`micro_iters_cpp_plugin.cpp`](cpp-plugin.md) (the `extern "C"` callback boundary that wires the model to the Benders framework).

## Main flow

```text
OnBendersStart (cpp-plugin.md)
  -> load CSV dictionaries, build the Plugin singleton, build the logger

OnBendersMasterResolutionEnd (cpp-plugin.md)
  -> rank 0: read binary investment decisions -> z_dict
  -> Plugin::compute_factors_for_micro_iterations (grid-modelisation/sensitivity-computation.md)
  -> broadcast factors + timing over MPI

OnBendersMicroIterationEnd (cpp-plugin.md)
  -> build F_N_values from the subproblem solution
  -> Plugin::return_constraints_for_micro_iteration (grid-modelisation/constraint-selection.md)
  -> deduplicate violated constraint families, append them to constraints_to_add_vec

  framework re-solves the subproblem with the new constraints,
  repeating until no violated constraint remains

OnBendersIterationStart (cpp-plugin.md)
  -> reset the per-subproblem "already added" tracking for the next Benders iteration
```

1. The framework calls [`OnBendersStart`](cpp-plugin.md), which loads the three CSV dictionaries, creates the [`Plugin`](grid-modelisation/index.md) singleton, and starts the [logger](logging.md).
2. After each master solve, [`OnBendersMasterResolutionEnd`](cpp-plugin.md) builds the investment decision map `z_dict` on rank 0 and calls [`compute_factors_for_micro_iterations`](grid-modelisation/sensitivity-computation.md) to recompute PTDF, HVDC sensitivities, and N-K incident factors for the new topology.
3. The factors are [broadcast](grid-modelisation/sensitivity-computation.md) to all MPI ranks.
4. Each subproblem is solved with a minimal constraint set; [`OnBendersMicroIterationEnd`](cpp-plugin.md) turns the solution into the flow map `F_N_values`.
5. [`return_constraints_for_micro_iteration`](grid-modelisation/constraint-selection.md) detects N and N-K violations and returns the worst violated constraint families.
6. Families already injected into that subproblem are deduplicated, and the remaining ones are appended to `constraints_to_add_vec`, which the framework adds to the subproblem.
7. The subproblem is re-solved; this micro-iteration loop stops when no new violated constraint is returned.
8. At the start of the next Benders iteration, [`OnBendersIterationStart`](cpp-plugin.md) resets the per-subproblem bookkeeping.

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [Architecture](architecture.md) | Components, build system, artifact, callback list | You need the big picture or want to build the plugin |
| [C++ plugin boundary](cpp-plugin.md) | `micro_iters_cpp_plugin.cpp`: callbacks, CSV dictionaries, singletons, dedup | You are tracing how the Benders framework drives the plugin |
| [Grid modelisation](grid-modelisation/index.md) | The `Plugin` class: grid data, PTDF/HVDC/N-K math, constraint selection | You need the core modeling and micro-iteration logic |
| [Configuration](configuration.md) | `plugin_params.json` knobs and the input files the plugin consumes | You want to tune tolerances/limits or find where a value comes from |
| [Logging](logging.md) | `MicroIterationsLog`, HTTP sender, `parse_micro_iterations.py` | You want to inspect or visualize micro-iteration output |
| [Glossary](glossary.md) | Definitions of Benders, PTDF, N-K, FPM and other terms | A domain term on any page is unclear |

## Where to start

- To understand what the plugin does end to end, read [Architecture](architecture.md).
- To understand the grid math, read [Grid modelisation](grid-modelisation/index.md) and its subpages.
- To understand how the Benders framework calls into the plugin, read [C++ plugin boundary](cpp-plugin.md).
- To tune the algorithm, read [Configuration](configuration.md).
