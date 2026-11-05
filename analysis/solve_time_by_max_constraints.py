"""Accumulated solve time per (Benders iteration, subproblem), one bar per max-constraints study.

Usage: python analysis/solve_time_by_max_constraints.py [out_dir] [study_dir ...]
  defaults: out_dir results/max_constraints_comparison/solve_time, every *_constraint* folder.

Time = sum of the solving_time column over the micro iterations of that sub, converted to
seconds. The logger documents the column as microseconds (wiki/logging.md) although some
logs label it "ms": the number is read as microseconds.
"""
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from max_constraints_common import collect, discover, grouped_axes, sub_num  # noqa: E402

US_TO_S = 1e-6


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "results/max_constraints_comparison/solve_time")
    studies = [Path(p) for p in sys.argv[2:]] or discover()
    out.mkdir(parents=True, exist_ok=True)
    data = collect(studies, lambda mi: mi.solving_time * US_TO_S)

    with (out / "solve_time.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["max_constraints", "master_iter", "sub", "solve_time_s"])
        for b in sorted(data):
            for m in sorted(data[b]):
                for s in sorted(data[b][m], key=sub_num):
                    w.writerow([b, m, s, f"{data[b][m][s]:.6f}"])
    print(f"{len(data)} studies, csv -> {out / 'solve_time.csv'}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    budgets, masters, subs, colours = grouped_axes(data, plt)
    ymax = max(v for d in data.values() for ms in d.values() for v in ms.values())
    w = 0.8 / len(budgets)
    for m in masters:
        fig, ax = plt.subplots(figsize=(16, 5))
        for j, b in enumerate(budgets):
            xs = [i + (j - (len(budgets) - 1) / 2) * w for i, s in enumerate(subs) if s in data[b].get(m, {})]
            ys = [data[b][m][s] for s in subs if s in data[b].get(m, {})]
            ax.bar(xs, ys, w, color=colours[b], label=str(b))
        ax.set_xticks(range(len(subs)))
        ax.set_xticklabels(subs, rotation=45, ha="right")
        ax.set_ylim(0, ymax * 1.05)
        ax.set_ylabel("accumulated solve time (s)")
        ax.set_title(f"Benders iteration {m}: solve time per subproblem, by max constraints")
        ax.legend(title="max constraints", ncol=len(budgets), fontsize=7, loc="upper center",
                  bbox_to_anchor=(0.5, -0.25))
        fig.tight_layout()
        fig.savefig(out / f"master_{m}.png", dpi=130)
        plt.close(fig)
    print(f"{len(masters)} figures -> {out}")


if __name__ == "__main__":
    main()
