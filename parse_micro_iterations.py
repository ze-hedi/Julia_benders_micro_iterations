"""Parse micro_iterations_proc_<rank>.log files of a study into a BendersLog."""
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

MASTER_RE = re.compile(
    r"master iteration (\d+) elapsed time : (\S*)\s*(?:ms)? ; "
    r"master resolution time : (\S+) s"
)


@dataclass
class MicroIteration:
    num_micro_iter: int
    nb_added_constraints: int
    solving_time: float  # us
    elapsed_time: float  # ms
    # (constraint family name, is_N) added at this micro iteration
    added_constraints: list[tuple[str, bool]] = field(default_factory=list)


def read_added_constraints(
    root: Path, num_master: int, sub_name: str, num_micro: int
) -> list[tuple[str, bool]]:
    """Read <root>/added_constraints/master_<i>/<sub>/micro_iter_<j>.txt (lines: "<name> <0|1>")."""
    sub_dir = Path(sub_name).name.removesuffix(".mps")  # same cleaning as the C++ dump
    f = root / "added_constraints" / f"master_{num_master}" / sub_dir / f"micro_iter_{num_micro}.txt"
    if not f.exists():
        return []
    result = []
    for line in f.read_text().splitlines():
        if line.strip():
            name, is_n = line.rsplit(" ", 1)
            result.append((name, is_n == "1"))
    return result


@dataclass
class BendersIteration:
    num_iter: int
    elapsed_time: float = 0.0  # ms, PTDF computation
    master_resolution_time: float = 0.0  # s
    # sub name -> micro iterations of that subproblem
    micro_iterations: dict[str, list[MicroIteration]] = field(default_factory=dict)


BendersLog = list[BendersIteration]


def parse_study(study_path: str) -> BendersLog:
    iterations: dict[int, BendersIteration] = {}

    def get(num_iter: int) -> BendersIteration:
        return iterations.setdefault(num_iter, BendersIteration(num_iter))

    for log_file in sorted(Path(study_path).rglob("micro_iterations_proc_*.log")):
        is_rank_0 = log_file.name == "micro_iterations_proc_0.log"
        for line in log_file.read_text().splitlines():
            m = MASTER_RE.match(line)
            if m:
                # master lines are identical on every rank: read them from rank 0 only
                if is_rank_0:
                    it = get(int(m.group(1)))
                    it.elapsed_time = float(m.group(2) or 0)
                    it.master_resolution_time = float(m.group(3))
                continue
            parts = [p.strip() for p in line.split(";")]
            if len(parts) != 6:  # header, blank lines
                continue
            sub_name, num_master, num_micro, nb, solving, elapsed = parts
            get(int(num_master)).micro_iterations.setdefault(sub_name, []).append(
                MicroIteration(
                    num_micro_iter=int(num_micro),
                    nb_added_constraints=int(nb),
                    solving_time=float(solving.removesuffix("us").removesuffix("ms")),  # older logs wrote ms
                    elapsed_time=float(elapsed.removesuffix("ms")),
                    added_constraints=read_added_constraints(
                        log_file.parent, int(num_master), sub_name, int(num_micro)
                    ),
                )
            )
    return [iterations[k] for k in sorted(iterations)]


if __name__ == "__main__":
    log = parse_study(sys.argv[1])
    for it in log:
        print(
            f"master {it.num_iter}: ptdf={it.elapsed_time} ms, "
            f"master={it.master_resolution_time} s, { {k: len(v) for k, v in it.micro_iterations.items()} } micro iters"
        )
