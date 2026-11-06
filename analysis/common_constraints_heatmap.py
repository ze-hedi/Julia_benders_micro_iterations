"""Overlap of the constraints added by each subproblem, one heatmap per Benders iteration.

For a master iteration, the constraints of a subproblem are the union of everything
added over its micro iterations. Cell (A, B) is the overlap between A and B.

Usage: python analysis/common_constraints_heatmap.py [study_dir] [out_dir]
         [--unit branch|family] [--metric jaccard|row]
  --unit    branch (default): monitored branch X, so branch_X and branch_X_inc_Y both give X
            family: exact family names (branch_X, branch_X_inc_Y, inc_Y)
  --metric  jaccard (default): |A&B| / |A|B|, symmetric
            row: |A&B| / |A|, fraction of the row subproblem's set also in the column one
Subproblems with an empty set have no defined overlap: their cells are NaN (grey).
"""
import argparse
import csv
import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from parse_micro_iterations import parse_study  # noqa: E402

BRANCH_RE = re.compile(r"branch_(.+?)(?:_inc_.+)?$")


def key(name: str, unit: str):
    if unit == "family":
        return name
    m = BRANCH_RE.match(name)
    return m.group(1) if m else None  # inc_<Y> families carry no monitored branch


def sets_per_master(study: str, unit: str):
    """{master: {sub: set}} with sub names like sub_10, ordered numerically."""
    out = {}
    for it in parse_study(study):
        d = {}
        for sub, micros in it.micro_iterations.items():
            name = Path(sub).name.removesuffix(".mps")
            d[name] = {k for mi in micros for n, _ in mi.added_constraints if (k := key(n, unit))}
        out[it.num_iter] = dict(sorted(d.items(), key=lambda kv: int(kv[0].split("_")[1])))
    return out


def overlap(a: set, b: set, metric: str) -> float:
    if not a or not b:
        return math.nan
    return 100 * len(a & b) / (len(a | b) if metric == "jaccard" else len(a))


def matrix(sets: dict, metric: str):
    names = list(sets)
    return names, [[overlap(sets[a], sets[b], metric) for b in names] for a in names]


def write_csv(path: Path, per_master, metric: str):
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["master_iter", "sub_row", "sub_col", f"overlap_pct_{metric}"])
        for m, sets in per_master.items():
            names, mat = matrix(sets, metric)
            for i, a in enumerate(names):
                for j, b in enumerate(names):
                    w.writerow([m, a, b, "" if math.isnan(mat[i][j]) else f"{mat[i][j]:.1f}"])


def plot(m, sets, metric, unit, path: Path, plt):
    import numpy as np
    names, mat = matrix(sets, metric)
    arr = np.array(mat, dtype=float)
    shown = arr.copy()
    np.fill_diagonal(shown, np.nan)  # diagonal is always 100: drawn as grey
    cmap = plt.get_cmap("viridis").copy()
    cmap.set_bad("#dddddd")
    fig, ax = plt.subplots(figsize=(9, 7.5))
    im = ax.imshow(np.ma.masked_invalid(shown), cmap=cmap, vmin=0, vmax=100)
    ticks = [n.split("_")[1] for n in names]
    ax.set_xticks(range(len(names)), ticks)
    ax.set_yticks(range(len(names)), ticks)
    ax.set_xlabel("subproblem")
    ax.set_ylabel("subproblem")
    for i in range(len(names)):
        for j in range(len(names)):
            if i != j and not np.isnan(arr[i, j]):
                ax.text(j, i, f"{arr[i, j]:.0f}", ha="center", va="center", fontsize=7,
                        color="white" if arr[i, j] < 60 else "black")
    what = "monitored branches" if unit == "branch" else "constraint families"
    pct = "Jaccard" if metric == "jaccard" else "% of row subproblem's set"
    ax.set_title(f"Master iteration {m}: common {what} ({pct}, %)")
    fig.colorbar(im, ax=ax, label="% in common")
    fig.tight_layout()
    fig.savefig(path, dpi=130)
    plt.close(fig)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("study", nargs="?", default="100_constraint")
    p.add_argument("out", nargs="?")
    p.add_argument("--unit", choices=["branch", "family"], default="branch")
    p.add_argument("--metric", choices=["jaccard", "row"], default="jaccard")
    a = p.parse_args()
    out = Path(a.out) if a.out else Path("results") / Path(a.study).name / f"common_{a.unit}_{a.metric}"
    out.mkdir(parents=True, exist_ok=True)
    per_master = sets_per_master(a.study, a.unit)
    write_csv(out / "overlap.csv", per_master, a.metric)
    print(f"{len(per_master)} master iterations, csv -> {out / 'overlap.csv'}")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    for m, sets in per_master.items():
        plot(m, sets, a.metric, a.unit, out / f"master_{m}.png", plt)
    print(f"{len(per_master)} heatmaps -> {out}")


if __name__ == "__main__":
    main()
