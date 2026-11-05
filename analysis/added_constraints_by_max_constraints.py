"""Added constraint families per (Benders iteration, subproblem), one stacked N/NK bar per study.

Usage: python analysis/added_constraints_by_max_constraints.py [out_dir] [study_dir ...]
  defaults: out_dir results/max_constraints_comparison/added_constraints, every *_constraint* folder.

Counts are families summed over the sub's micro iterations (same unit as the budget):
N = flag 1 (solid), NK = flag 0 (hatched, stacked on N).
"""
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from max_constraints_common import collect, discover, grouped_axes, sub_num  # noqa: E402


def count(mi):
    n = sum(1 for _, is_n in mi.added_constraints if is_n)
    return (n, len(mi.added_constraints) - n)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "results/max_constraints_comparison/added_constraints")
    studies = [Path(p) for p in sys.argv[2:]] or discover()
    out.mkdir(parents=True, exist_ok=True)
    data = collect(studies, count)

    with (out / "added_constraints.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["max_constraints", "master_iter", "sub", "n_N", "n_NK"])
        for b in sorted(data):
            for m in sorted(data[b]):
                for s in sorted(data[b][m], key=sub_num):
                    w.writerow([b, m, s, *data[b][m][s]])
    print(f"{len(data)} studies, csv -> {out / 'added_constraints.csv'}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch
    budgets, masters, subs, colours = grouped_axes(data, plt)
    ymax = max(sum(v) for d in data.values() for ms in d.values() for v in ms.values())
    w = 0.8 / len(budgets)
    for m in masters:
        fig, ax = plt.subplots(figsize=(16, 5))
        for j, b in enumerate(budgets):
            present = [(i, data[b][m][s]) for i, s in enumerate(subs) if s in data[b].get(m, {})]
            xs = [i + (j - (len(budgets) - 1) / 2) * w for i, _ in present]
            ns = [v[0] for _, v in present]
            nks = [v[1] for _, v in present]
            ax.bar(xs, ns, w, color=colours[b])
            ax.bar(xs, nks, w, bottom=ns, color=colours[b], hatch="////", edgecolor="white", lw=0.3, alpha=0.75)
        ax.set_xticks(range(len(subs)))
        ax.set_xticklabels(subs, rotation=45, ha="right")
        ax.set_ylim(0, ymax * 1.05)
        ax.set_ylabel("constraint families added (N + NK)")
        ax.set_title(f"Benders iteration {m}: added constraints per subproblem, by max constraints")
        handles = [Patch(color=colours[b], label=str(b)) for b in budgets]
        handles += [Patch(facecolor="grey", label="N"), Patch(facecolor="grey", hatch="////", edgecolor="white", label="NK (stacked)")]
        ax.legend(handles=handles, title="max constraints", ncol=len(handles), fontsize=7, loc="upper center",
                  bbox_to_anchor=(0.5, -0.25))
        fig.tight_layout()
        fig.savefig(out / f"master_{m}.png", dpi=130)
        plt.close(fig)
    print(f"{len(masters)} figures -> {out}")


if __name__ == "__main__":
    main()
