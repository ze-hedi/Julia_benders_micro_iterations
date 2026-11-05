"""Shared helpers for the comparisons across max-constraints studies (<n>_constraint folders)."""
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from parse_micro_iterations import parse_study  # noqa: E402

WORDS = {"one": 1, "two": 2, "three": 3}


def study_budget(path: Path) -> int:
    """'100_constraint' -> 100, 'one_constraint' -> 1, 'three_constraints' -> 3."""
    head = path.name.split("_")[0]
    return int(head) if head.isdigit() else WORDS[head]


def discover(root: Path = Path(".")) -> list[Path]:
    found = [p for p in root.glob("*_constraint*") if (p / "micro_iterations_proc_0.log").exists()]
    return sorted(found, key=study_budget)


def sub_num(sub: str) -> int:
    return int(re.search(r"(\d+)", sub).group(1))


def collect(studies, per_micro):
    """Return {budget: {master: {sub: value}}}, value = sum over micro iterations of per_micro(mi)."""
    out = {}
    for s in studies:
        per_master = defaultdict(dict)
        for it in parse_study(str(s)):
            for sub, micros in it.micro_iterations.items():
                name = Path(sub).name.removesuffix(".mps")
                vals = [per_micro(mi) for mi in micros]
                per_master[it.num_iter][name] = tuple(map(sum, zip(*vals))) if isinstance(vals[0], tuple) else sum(vals)
        out[study_budget(s)] = dict(per_master)
    return out


def grouped_axes(data, plt):
    """Common layout: returns (budgets, masters, subs, colour per budget)."""
    budgets = sorted(data)
    masters = sorted({m for d in data.values() for m in d})
    subs = sorted({s for d in data.values() for ms in d.values() for s in ms}, key=sub_num)
    cmap = plt.get_cmap("viridis")
    colours = {b: cmap(i / max(1, len(budgets) - 1)) for i, b in enumerate(budgets)}
    return budgets, masters, subs, colours
