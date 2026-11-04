# Glossary

**Summary:** Definitions of the electrical-grid and Benders-decomposition terms used across the code, so the math pages stay readable.
**Parent:** [Entry](entry.md)
**Related:** [Grid modelisation](grid-modelisation/index.md), [Architecture](architecture.md)

## Purpose

This page defines the electrical-grid and Benders-decomposition terms used on the other pages. It has no executable code of its own; it exists so the math and data-flow pages can stay concise and link here instead of re-explaining domain vocabulary.

## Benders decomposition

An iterative optimization method that splits a problem into a **master** problem (a MILP that picks discrete decisions, here the candidate transmission lines to build) and one or more **subproblems** (continuous operational problems, here the electrical grid dispatch). The master proposes investments; the subproblems evaluate them and send back feasibility/optimality cuts until upper and lower bounds converge.

## Master problem

The MILP solved by the Benders framework that selects candidate lines. Its binary investment decisions are what the plugin reads in `OnBendersMasterResolutionEnd` as `master_out` and turns into the `z_dict` map.

## Subproblem

A continuous optimization problem solved for a given master decision. Each subproblem corresponds to a scenario (identified by a name such as `sub/sub_9.mps`) and represents the grid dispatch under that scenario. Micro-iterations refine a subproblem's constraint set.

## Micro-iteration

The inner loop that refines a single subproblem. The subproblem starts with a minimal constraint set, is solved, and the plugin detects violated flow constraints. The worst violations are injected, and the subproblem is re-solved. The loop ends when no new violated constraint remains.

## Constraint family

A named group of constraints returned by the plugin, such as `branch_LINE-103-109` (an N-regime limit on one branch) or `branch_LINE-107-108_inc_LINE-103-109` plus `inc_LINE-103-109` (an N-K contingency limit). `constraints_dictionary.csv` maps a family name to the actual constraint identifiers the framework must add. See [Configuration](configuration.md) and [Constraint selection](grid-modelisation/constraint-selection.md).

## PTDF (Power Transfer Distribution Factor)

A matrix that maps nodal power injections to line flows. In the code it is computed as `PTDF = Yl * Ab * B_inv` (rows are lines, columns are nodes). See [Sensitivity computation](grid-modelisation/sensitivity-computation.md).

## N regime and N-K regime

- **N** (intact network): the grid with all its lines in service. A flow limit `max_flows_N` applies per branch and scenario.
- **N-K** (contingency): the grid after an incident (one or more lines out of service). The plugin models each incident as a list of outaged AC branches plus outaged HVDC branches, with a per-incident limit `max_flows_N_K`.

## Incident / outage

A contingency scenario, identified by a key such as `LINE-103-109`. `dict_incident_outage_AC_branches.json` maps an incident id to the list of AC branches taken out; `dict_incident_HVDC_branches.json` maps it to the outaged HVDC branches.

## Incident factor

A post-contingency sensitivity: how much the pre-contingency flow on one outaged line changes the flow on a monitored line after the outage. Computed from the FPM matrix. See [Sensitivity computation](grid-modelisation/sensitivity-computation.md).

## FPM (factor matrix)

In the code, `FPM = (I - (PTDF_side1 - PTDF_side2))^-1`, the inverse of the identity minus a PTDF difference sub-matrix for the set of outaged lines. It is the generalized LODF-style post-contingency redistribution matrix used to compute incident factors. Despite the name, it is a sensitivity matrix, not a probability.

## LODF (Line Outage Distribution Factor)

The standard power-flow term for how much a line outage redistributes flow onto other lines. The code's `FPM` plays this role for multi-line contingencies.

## Woodbury update

The Sherman-Morrison-Woodbury identity, used in `update_PTDF_after_lines_removal` to update `B_inv` (the inverse nodal susceptance matrix) when lines are removed, without re-factorizing. See [Sensitivity computation](grid-modelisation/sensitivity-computation.md).

## HVDC

High-voltage direct-current links. `A_hvdc` maps HVDC injections to nodes; the HVDC sensitivity matrix (`PTDF_new * A_hvdc`) maps HVDC injections to line flows, used in the N-K overflow formula.

## Susceptance matrix

`B_inv` is the inverse of the nodal susceptance matrix of the DC power-flow model. Its inverse (`B`) captures how nodal injections drive node voltage angles; `B_inv` is updated by the Woodbury formula when lines are removed.

## MPI

Message Passing Interface. The plugin uses it to broadcast the recomputed factors from rank 0 to all workers (`boost::mpi`).
