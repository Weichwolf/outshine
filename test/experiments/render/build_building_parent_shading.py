"""Measure original-source parent creation and subsequent offline shading states."""

import argparse
import json
import subprocess
import time
from pathlib import Path

from build_building_hierarchy import build_case
from build_building_hierarchy_growth import growth_queries, population_cells
from building_mass_materials import policy
from fit_building_parent_shading import compile_states
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    cpu, wall = time.process_time(), time.perf_counter()
    with open_parent(args.input) as parent:
        cells = population_cells(parent.plans, args.seed_cell, args.cell_count)
        case = build_case(
            parent,
            cells,
            args.output / "parents",
            policy(),
            make_queries=growth_queries,
        )
        (args.output / "parents/report.json").write_text(
            json.dumps(case, indent=2) + "\n"
        )
        digest = parent.archive_sha256
    tables = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    fitted = compile_states(args.output / "parents", args.output / "fits", tables)
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_archive_sha256": digest,
        "parents": case,
        "shading_states": fitted,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Sequential measured original archive/index, scalar parent recipe, source-material production, then fixed1/2-lobe offline shading-state controls. Raw source/parent assets retained. Stage process peaks cumulative; total producer measured directly, not an addition of async times. No rendering/loading solver, new cache/asset/native binding or device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["cost"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--seed-cell", type=int, required=True)
    parser.add_argument("--cell-count", type=int, required=True)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
