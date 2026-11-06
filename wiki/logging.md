# Logging

**Summary:** `MicroIterationsLog` writes per-rank text logs and per-micro-iteration constraint dumps on a background thread so the solver loop is never blocked by I/O. An optional raw-socket HTTP sender can POST the same data to a frontend, and `parse_micro_iterations.py` turns the logs back into structured data.
**Main code:** `utils/micro_iterations_logger.h`, `utils/micro_iterations_logger.cpp`, `utils/micro_iterations_http_sender.h`, `utils/micro_iterations_http_sender.cpp`, `parse_micro_iterations.py`
**Parent:** [Entry](entry.md)
**Related:** [C++ plugin boundary](cpp-plugin.md), [Architecture](architecture.md)

## Purpose

The plugin must not slow down the Benders solver with disk or network I/O. The logger therefore runs its own worker thread behind a task queue: the solver thread enqueues a lambda and returns immediately, and the worker drains the queue and performs the actual file writes. This lets the callbacks record timing and the added constraints without blocking.

## `MicroIterationsLog`

Constructed in `OnBendersStart` with `(output_root, warm_start, world, log_level)`. On construction it:

- opens `micro_iterations_proc_<rank>.log` under `output_root_` and writes a header plus `warm_start=0`/`1`;
- on rank 0, creates (or clears and recreates) `output_root_/added_constraints/`;
- starts the file worker thread.

The HTTP worker thread is started here too, so the logging methods also POST to the frontend server (`127.0.0.1:3123`).

### Methods

| Method | Writes |
|---|---|
| `AddMasterIterationLog` | `master iteration <n> elapsed time : <ms> ms ; master resolution time : <s> s` |
| `AddMicroIterionLog` | `<sub> ; <master> ; <micro> ; <nb_added> ; <solving_time> us ; <elapsed> ms ; <n_saturated_constraints> ; <n_all_constraints>` |
| `AddMicroIterCount` | `<sub> ; <micro>` |
| `DumpAddedConstraints` | `added_constraints/master_<m>/<sub>/micro_iter_<j>.txt`, lines `<name> <0|1>` |

`DumpAddedConstraints` cleans the subproblem name by taking the part after the last `/` and stripping a trailing `.mps`. It writes only the slice `[start, end)` of the family list, so each micro-iteration file contains exactly the families added in that micro iteration (the caller passes `size_before`/`size_after`).

### Threading

`enqueue` pushes a task onto `task_queue_` and notifies a condition variable. `workerLoop` waits for tasks, pops one, and runs it. On destruction the logger sets `stop_`, notifies, and joins the worker, which first drains any remaining queued tasks. The class is non-copyable and non-movable because it owns the thread.

The HTTP side mirrors this with a second queue (`http_queue_`) and thread (`httpWorkerLoop`), and `enqueueHttp` is called from `AddMasterIterationLog` (rank 0 only, `/master_times`), `AddMicroIterionLog` (`/micro_iter`) and `DumpAddedConstraints` (`/added_constraints`).

## `MicroIterationsHttpSender`

A minimal raw-POSIX-socket HTTP client. It opens a fresh TCP connection per request, writes a `POST <path> HTTP/1.1` request with a JSON body and `Connection: close`, then closes the socket without reading a response. Failures are reported on `stderr` and the return value is ignored, so a slow or absent server never blocks or aborts the solver. It is configured for `127.0.0.1:3123`. The intended endpoints (`/micro_iter`, `/master_times`, `/added_constraints`) are commented out in the logger alongside the disabled HTTP thread.

## `parse_micro_iterations.py`

A standalone script that parses the `micro_iterations_proc_*.log` files of a study into a `BendersLog` (a list of `BendersIteration`, each with master timing and a dict of subproblem -> `MicroIteration` objects). It reads master lines from rank 0 only, and reconstructs the per-micro-iteration added constraints by reading the `added_constraints/...` dump files written by `DumpAddedConstraints`. This is the "object to help visualize the info" mentioned in the commit history.
