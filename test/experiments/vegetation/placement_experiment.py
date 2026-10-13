"""Verify complete length-adaptive graph products and compare equal site workloads."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from placement_checks import check_graph, check_patterns, pattern_cases, scalar_tables
from placement_patterns import DistancePattern
from placement_scenes import make_scenes
from profile_curve import Profile


def check_sparse():
    lengths = np.zeros(65536)
    lengths[-1] = 1
    pattern = DistancePattern(spacing=Profile(np.full(4, 1 / 4095)), limit_groups=4096)
    allocated = []
    original = np.full

    def bounded(shape, *args, **kwargs):
        count = int(np.prod(shape))
        assert count <= 65536
        allocated.append(count)
        return original(shape, *args, **kwargs)

    with patch("placement_patterns.np.full", side_effect=bounded):
        tables = pattern.tables(lengths, 0, 1, max_sites=4096)
    assert sum(len(t) for t in tables) == 4096
    assert tables[0] is tables[65534]
    np.testing.assert_allclose(tables[-1][:, 0], 1 - np.arange(4096) / 4095, atol=2e-15)
    return {
        "sparse_parents": len(lengths),
        "sparse_sites": 4096,
        "maximum_full_allocation": max(allocated),
    }


def measure():
    lengths = np.linspace(0.2, 6, 256)
    result = []
    for source in (pattern_cases()[2], pattern_cases()[3]):
        timings = {"scalar": [], "batch": []}
        outputs = {}
        for _ in range(5):
            for method in ("scalar", "batch"):
                began = time.perf_counter()
                outputs[method] = (
                    scalar_tables(source, lengths, 0.15, 0.95)
                    if method == "scalar"
                    else source.tables(lengths, 0.15, 0.95)
                )
                timings[method].append(time.perf_counter() - began)
        for a, b in zip(outputs["scalar"], outputs["batch"]):
            np.testing.assert_allclose(a, b, atol=3e-13, rtol=0)
        result.append(
            {
                "spacing_profile": source.spacing.table.tolist(),
                "parents": len(lengths),
                "sites": sum(len(a) for a in outputs["batch"]),
                "samples_s": timings,
                "retained_site_bytes": sum(a.nbytes for a in outputs["batch"]),
            }
        )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/placements")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    checks = (
        check_patterns()
        | check_sparse()
        | {"graph": check_graph(args.output / "checks")}
    )
    costs = measure()
    began = time.perf_counter()
    scenes = make_scenes(args.output)
    products_s = time.perf_counter() - began
    source_names = (
        "placement_patterns.py",
        "placement_checks.py",
        "placement_scenes.py",
        "placement_experiment.py",
        "generator_specs.py",
        "generator_graph.py",
        "graph_frames.py",
        "spatial_axis.py",
        "spine_measure.py",
        "spine_field.py",
        "bend_pattern.py",
        "graph_products.py",
        "graph_sources.py",
    )
    files = [Path(__file__).with_name(name) for name in source_names]
    report = {
        "checks": checks,
        "costs": costs,
        "scenes": scenes,
        "products_s": products_s,
        "process_peak_with_checks_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "native_or_modeler_equivalence_proven": False,
    }
    (args.output / "placements.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("checks", "scenes", "products_s")}))


if __name__ == "__main__":
    main()
