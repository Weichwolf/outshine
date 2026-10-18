"""Fresh trusted compact-parameter reader without any original source file."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

from compact_crown import CompactCrown
from compact_crown_probe import output_digest
from crown_demand_probe import cameras


def read(parameters, expected_source, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    initial_peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    with (
        patch.object(
            CompactCrown, "build", side_effect=AssertionError("parameter build")
        ),
        patch(
            "spatial_group_source.load_source",
            side_effect=AssertionError("source read"),
        ),
        patch("numpy.random.default_rng", side_effect=AssertionError("randomness")),
        patch(
            "crown_shoots.CrownShoots.group", side_effect=AssertionError("source poses")
        ),
    ):
        before = time.perf_counter()
        product = CompactCrown.load(parameters, expected_source=expected_source)
        load_s = time.perf_counter() - before
        cases = []
        for name, camera in cameras().items():
            before = time.perf_counter()
            selected = camera.visible(product.data["lower"], product.data["upper"])
            poses = product.evaluate(selected, max_leaves=100000)
            elapsed = time.perf_counter() - before
            cases.append(
                {
                    "view": name,
                    "leaves": len(poses["ids"]),
                    "select_evaluate_s": elapsed,
                    "output_sha256": output_digest(poses),
                    "output_array_bytes": sum(v.nbytes for v in poses.values()),
                }
            )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": expected_source,
        "load_s": load_s,
        "file_bytes": parameters.stat().st_size,
        "parameter_array_bytes": sum(v.nbytes for v in product.data.values()),
        "cases": cases,
        "initial_rusage_peak_rss_bytes_linux": initial_peak,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "proc_self_high_water_rss_bytes_linux": next(
            int(line.split()[1]) * 1024
            for line in Path("/proc/self/status").read_text().splitlines()
            if line.startswith("VmHWM:")
        ),
        "scope": "Fresh offline Python process; only trusted finished parameters loaded, no original source inputs/build/RNG/group callbacks. CPU expanded pose comparator outputs, no GPU/native/production cache format or frame-budget claim.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parameters", type=Path, required=True)
    parser.add_argument("--expected-source", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    read(args.parameters, args.expected_source, args.output)
