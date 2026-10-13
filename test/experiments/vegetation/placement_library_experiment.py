"""Same-input site-table library comparison, complete budgets and graph regression."""

import argparse
import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from dataclasses import asdict
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import DistancePattern, Profile
from placement_checks import check_graph, check_patterns
from placement_experiment import check_sparse
from placement_scenes import make_scenes


def load_before(path):
    spec = importlib.util.spec_from_file_location("placement_before", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module.DistancePattern


def before_pattern(pattern, before_type):
    values = asdict(pattern)
    values["spacing"] = pattern.spacing
    return before_type(**values)


def compare(name, lengths, pattern, before_type, max_sites=1_000_000):
    before = before_pattern(pattern, before_type)
    samples = {"before": [], "after": []}
    for repeat in range(5):
        outputs = {}
        for key in ("before", "after") if repeat % 2 == 0 else ("after", "before"):
            started = time.perf_counter()
            outputs[key] = (before if key == "before" else pattern).tables(
                lengths, 0.15, 0.95, max_sites=max_sites, seeds=np.arange(len(lengths))
            )
            samples[key].append(time.perf_counter() - started)
        if outputs["before"] is None:
            assert outputs["after"] is None
        else:
            for a, b in zip(outputs["before"], outputs["after"], strict=True):
                np.testing.assert_array_equal(a, b)
    sites = 0 if outputs["after"] is None else sum(map(len, outputs["after"]))
    return {
        "name": name,
        "parents": len(lengths),
        "sites": sites,
        "samples_s": samples,
        "arrays_exact": True,
        "budget_refused": outputs["after"] is None,
        "site_bytes": sites * 2 * np.dtype(np.float64).itemsize,
    }


def compare_products(before, after):
    count = 0
    for path in before.glob("**/*.npz"):
        with np.load(path) as a, np.load(after / path.relative_to(before)) as b:
            assert a.files == b.files
            for name in a.files:
                np.testing.assert_array_equal(a[name], b[name])
        count += 1
    assert count > 0
    return count


def graph_regression(root, before_type):
    before = root / "graph-before"
    after = root / "graph-after"
    with patch.object(DistancePattern, "tables", before_type.tables):
        old = check_graph(before)
    new = check_graph(after)
    assert old == new
    return new | {"products_npz_exact": compare_products(before, after)}


def scene_regression(root, before_type):
    before = root / "scenes-before"
    after = root / "scenes-after"
    with patch.object(DistancePattern, "tables", before_type.tables):
        old = make_scenes(before)
    new = make_scenes(after)
    assert old == new
    count = compare_products(before, after)
    current = Path("build/experiments/vegetation/placements")
    buffers = compare_current_scenes(after, current)
    return {
        "scene_modes": len(new),
        "products_npz_exact": count,
        "current_showcase_buffers_exact": buffers,
    }


def compare_current_scenes(after, current):
    count = 0
    for path in after.glob("*/placement-*.json"):
        a = json.loads(path.read_text())
        original = current / path.relative_to(after)
        b = json.loads(original.read_text())
        assert a["plan_identity"] == b["plan_identity"]
        assert json.loads(
            (path.parent / "bark-material.json").read_text()
        ) == json.loads((original.parent / "bark-material.json").read_text())
        old_products = {
            (p["surface"], tuple(p["generator_ids"])): p for p in b["products"]
        }
        assert len(old_products) == len(a["products"])
        for product in a["products"]:
            previous = old_products[
                (product["surface"], tuple(product["generator_ids"]))
            ]
            assert product["instances"] == previous["instances"]
            for key in ("mesh", "placements"):
                with (
                    np.load(path.parent / product[key]) as x,
                    np.load(original.parent / previous[key]) as y,
                ):
                    assert x.files == y.files
                    for name in x.files:
                        np.testing.assert_array_equal(x[name], y[name])
                count += 1
    assert count > 0
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/vegetation/placement-library"),
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    before = load_before(args.output / "pattern.before.py")
    constant = DistancePattern(mode="interval", whorl_size=3, divergence=0.4)
    profiled = DistancePattern(
        spacing=Profile(np.array([0.03, 0.06, 0.10, 0.04])),
        whorl_size=3,
        divergence=0.7,
        spread_m=0.003,
    )
    sparse = np.zeros(65536)
    sparse[-1] = 1
    checks = check_patterns() | check_sparse()
    checks["graph"] = graph_regression(args.output, before)
    checks["showcase"] = scene_regression(args.output, before)
    cases = [
        compare("constant", np.linspace(0.2, 6, 256), constant, before),
        compare("profiled-spread", np.linspace(0.2, 6, 256), profiled, before),
        compare(
            "sparse",
            sparse,
            DistancePattern(spacing=Profile(np.full(4, 0.8 / 4095))),
            before,
        ),
        compare("refused", np.ones(65536), DistancePattern(), before, max_sites=0),
    ]
    sources = [
        Path(__file__),
        Path("lib/src/outshine_ref/vegetation/placement_patterns.py"),
        args.output / "pattern.before.py",
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "test/experiments/vegetation",
                    "lib/src/outshine_ref/vegetation",
                ],
                text=True,
            ).strip()
        ),
        "checks": checks,
        "cases": cases,
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources
        },
        "native_lod_or_device_proven": False,
    }
    (args.output / "placement-library.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
