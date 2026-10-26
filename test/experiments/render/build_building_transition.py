"""Build existing source-tree products immediately either side of the root cut."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from build_building_hierarchy import case
from building_mass_materials import policy
from building_tree_queries import queries
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes


def transition_queries(tree):
    low, high = tree["node_bounds"][0]
    centre, radius = (low + high) / 2, np.linalg.norm(high - low) / 2
    result = []
    for azimuth, template in enumerate(queries(tree)[::3]):
        threshold = radius + 2 * radius * template["focal_px"] / 8
        forward = np.array(template["basis"])[2]
        for name, factor in (("finer", 1 - 1e-4), ("coarser", 1 + 1e-4)):
            distance = threshold * factor
            result.append(
                template
                | {
                    "name": f"az{azimuth}-{name}",
                    "eye": (centre - forward * distance).tolist(),
                    "distance_m": float(distance),
                    "render_eye": (centre - forward * threshold).tolist(),
                    "render_distance_m": float(threshold),
                }
            )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    with open_parent(args.input) as parent:
        reports = [
            case(
                parent,
                cell,
                args.output / f"cell-{cell}",
                policy(),
                make_queries=transition_queries,
            )
            for cell in args.cells
        ]
        result = {
            "producer": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "source_archive_sha256": parent.archive_sha256,
            "cells": reports,
            "scope": "Declared testdriver root-threshold probes, selection eyes±0.01percent but identical render eye. Same camera-independent source-tree/node recipe and strict fallback. No free pose sweep, native Ready/LOD/cache change or source/fine-mesh export.",
        }
    result["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "peak_bytes": peak_rss_bytes(),
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cells": [
                    {
                        "cell": c["cell"],
                        "queries": [
                            {
                                "name": q["name"],
                                "nodes": q["nodes"],
                                "triangles": q["proxy_triangles"],
                                "original_needs": q["original_needs"],
                            }
                            for q in c["queries"]
                        ],
                    }
                    for c in reports
                ]
            }
        )
    )


if __name__ == "__main__":
    main()
