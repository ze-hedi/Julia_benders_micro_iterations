"""Overlap of the constraints one subproblem added at different Benders iterations.

One heatmap per subproblem: both axes are master iterations, cell (i, j) is the overlap
between what the subproblem added at iteration i and at iteration j (union over its
micro iterations). Same --unit / --metric options as common_constraints_heatmap.py.
Iterations where the subproblem added nothing are NaN (grey).

Usage: python analysis/common_constraints_across_iterations.py [study_dir] [out_dir]
         [--unit branch|family] [--metric jaccard|row]
"""
import argparse
import csv
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common_constraints_heatmap import matrix, sets_per_master  # noqa: E402


def sets_per_sub(study: str, unit: str):
    """{sub: {master: set}}"""
    out = {}
    for m, subs in sets_per_master(study, unit).items():
        for sub, s in subs.items():
            out.setdefault(sub, {})[m] = s
    return dict(sorted(out.items(), key=lambda kv: int(kv[0].split("_")[1])))


def write_csv(path: Path, per_sub, metric: str):
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["sub", "master_row", "master_col", f"overlap_pct_{metric}"])
        for sub, sets in per_sub.items():
            its, mat = matrix(sets, metric)
            for i, a in enumerate(its):
                for j, b in enumerate(its):
                    w.writerow([sub, a, b, "" if math.isnan(mat[i][j]) else f"{mat[i][j]:.1f}"])


def plot(sub, sets, metric, unit, path: Path, plt):
    import numpy as np
    its, mat = matrix(sets, metric)
    arr = np.array(mat, dtype=float)
    shown = arr.copy()
    np.fill_diagonal(shown, np.nan)  # diagonal is always 100: drawn as grey
    cmap = plt.get_cmap("viridis").copy()
    cmap.set_bad("#dddddd")
    fig, ax = plt.subplots(figsize=(7.5, 6.5))
    im = ax.imshow(np.ma.masked_invalid(shown), cmap=cmap, vmin=0, vmax=100)
    ax.set_xticks(range(len(its)), its)
    ax.set_yticks(range(len(its)), its)
    ax.set_xlabel("master iteration")
    ax.set_ylabel("master iteration")
    for i in range(len(its)):
        for j in range(len(its)):
            if i != j and not np.isnan(arr[i, j]):
                ax.text(j, i, f"{arr[i, j]:.0f}", ha="center", va="center", fontsize=8,
                        color="white" if arr[i, j] < 60 else "black")
    what = "monitored branches" if unit == "branch" else "constraint families"
    pct = "Jaccard" if metric == "jaccard" else "% of row iteration's set"
    ax.set_title(f"{sub}: common {what} between iterations ({pct}, %)", fontsize=10)
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
    out = Path(a.out) if a.out else Path("results") / Path(a.study).name / f"across_iterations_{a.unit}_{a.metric}"
    out.mkdir(parents=True, exist_ok=True)
    per_sub = sets_per_sub(a.study, a.unit)
    write_csv(out / "overlap.csv", per_sub, a.metric)
    print(f"{len(per_sub)} subproblems, csv -> {out / 'overlap.csv'}")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    for sub, sets in per_sub.items():
        plot(sub, sets, a.metric, a.unit, out / f"{sub}.png", plt)
    print(f"{len(per_sub)} heatmaps -> {out}")


if __name__ == "__main__":
    main()
