"""Compare exact source work with whole-cell work at the same fixed Fine LOD.

Coverage intentionally differs: the request names a subset, while the legacy
cell bake also builds its neighbours. These timings are not World frame times.
"""

import argparse
import hashlib
import json
import statistics
import subprocess
from collections import Counter
from pathlib import Path


def compare(args):
    cells = args.cells.read_bytes()
    supported = args.supported.read_bytes()
    if not cells or len(cells) != len(supported):
        raise ValueError("source cohort and support columns must have equal nonzero length")
    if any(cell < 1 or cell > 64 for cell in cells) or any(x > 1 for x in supported):
        raise ValueError("source cohort or support column contains invalid values")
    populations = Counter(cell for cell, support in zip(cells, supported) if support)
    if not populations:
        raise ValueError("no supported native source exists")
    cell = populations.most_common(1)[0][0]
    selected = [at for at, (cohort, support) in enumerate(zip(cells, supported))
                if cohort == cell and support][:args.sources]
    rows = []
    for _ in range(args.trials):
        for mode in ("subset", "cell"):
            command = [str(args.native.resolve()), str(args.asset_db.resolve()), args.base,
                       mode, str(args.triangle_attempts), *map(str, selected)]
            done = subprocess.run(command, text=True, capture_output=True, timeout=30, check=False)
            if done.returncode:
                raise RuntimeError(f"native {mode} failed ({done.returncode}): {done.stderr[:1000]}")
            result = json.loads(done.stdout)
            if result["unsupported_meshes"]:
                raise ValueError("partial unsupported geometry is not a valid cost comparison")
            if result["parent_sources"] != len(cells):
                raise ValueError("native base and supplied cohort column disagree")
            if mode == "subset" and result["baked_sources"] != len(selected):
                raise ValueError("the selected native group lost source members")
            rows.append({"command": command, **result})
    summary = {}
    for mode in ("subset", "cell"):
        cases = [row for row in rows if row["mode"] == mode]
        summary[mode] = {key: statistics.median(row[key] for row in cases)
                         for key in ("bake_ms", "baked_sources", "triangles", "output_heap_bytes",
                                     "coordinate_heap_bytes", "form_cell_reads", "form_bytes")}
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=Path(__file__).resolve().parents[3], text=True
    ).strip()
    report = {"commit": commit,
              "native_sha256": hashlib.sha256(args.native.read_bytes()).hexdigest(),
              "base": args.base, "fixed_lod": "Fine", "cell": cell,
              "sources": selected, "triangle_attempt_budget": args.triangle_attempts,
              "summary": summary, "trials": rows}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"base": args.base, "requested_sources": len(selected), **summary}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--asset-db", type=Path, required=True)
    parser.add_argument("--base", required=True)
    parser.add_argument("--cells", type=Path, required=True)
    parser.add_argument("--supported", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sources", type=int, default=8)
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--triangle-attempts", type=int, required=True)
    args = parser.parse_args()
    if args.sources < 1 or args.trials < 1 or args.triangle_attempts < 1:
        parser.error("source count, trials and triangle attempts must be positive")
    compare(args)


if __name__ == "__main__":
    main()
