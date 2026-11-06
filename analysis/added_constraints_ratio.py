"""N vs N-K constraint families added per micro iteration, one figure per subproblem.

Usage: python analysis/added_constraints_ratio.py [study_dir] [out_dir]
  study_dir defaults to 100_constraint, out_dir to results/<study_dir name>.

NK counts every N-K family (flag 0: branch_<X>_inc_<Y> and inc_<Y>), N counts the
branch_<X> families (flag 1): the same unit as the max_constraints budget.
"""
import csv
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from parse_micro_iterations import parse_study  # noqa: E402


def collect(study: str):
    """Return {sub: [(master, micro, n_N, n_NK), ...]} ordered by (master, micro)."""
    data = defaultdict(list)
    for it in parse_study(study):
        for sub, micros in it.micro_iterations.items():
            name = Path(sub).name.removesuffix(".mps")
            for mi in micros:
                n = sum(1 for _, is_n in mi.added_constraints if is_n)
                data[name].append((it.num_iter, mi.num_micro_iter, n, len(mi.added_constraints) - n))
    return {s: sorted(v) for s, v in sorted(data.items(), key=lambda kv: int(kv[0].split("_")[1]))}


def write_csv(data, path: Path):
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["sub", "master_iter", "micro_iter", "n_N", "n_NK"])
        for sub, rows in data.items():
            for r in rows:
                w.writerow([sub, *r])


def plot_sub(sub, rows, ymax, path: Path, plt):
    fig, ax = plt.subplots(figsize=(14, 4.5))
    w = 0.4
    masters = sorted({r[0] for r in rows})
    for i, (_, _, n, nk) in enumerate(rows):
        ax.bar(i - w / 2, n, w, color="tab:blue", label="N" if i == 0 else None)
        ax.bar(i + w / 2, nk, w, color="tab:orange", label="NK" if i == 0 else None)
    ax.set_xticks(range(len(rows)))
    ax.set_xticklabels([str(r[1]) for r in rows], fontsize=7)
    # master iteration separators and labels
    for m in masters:
        idx = [i for i, r in enumerate(rows) if r[0] == m]
        if idx[0] > 0:
            ax.axvline(idx[0] - 0.5, color="grey", lw=0.8, ls="--")
        ax.text((idx[0] + idx[-1]) / 2, ymax * 1.02, f"master {m}", ha="center", va="bottom", fontsize=8)
    ax.set_ylim(0, ymax * 1.1)
    ax.set_xlabel("micro iteration (grouped by master iteration)")
    ax.set_ylabel("constraint families added")
    ax.set_title(f"{sub}: N vs NK added constraints", pad=22)
    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(path, dpi=130)
    plt.close(fig)


def main():
    study = Path(sys.argv[1] if len(sys.argv) > 1 else "100_constraint")
    out = Path(sys.argv[2] if len(sys.argv) > 2 else Path("results") / study.name)
    out.mkdir(parents=True, exist_ok=True)
    data = collect(str(study))
    write_csv(data, out / "n_vs_nk.csv")
    print(f"{len(data)} subproblems, csv -> {out / 'n_vs_nk.csv'}")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    ymax = max(max(r[2], r[3]) for rows in data.values() for r in rows)
    for sub, rows in data.items():
        plot_sub(sub, rows, ymax, out / f"{sub}.png", plt)
    print(f"{len(data)} figures -> {out}")


if __name__ == "__main__":
    main()
