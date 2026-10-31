"""Whole fixed-cohort interior-fan alternatives with exact continuous error.

No runtime mesh, screen-space tolerance, shader or World admission is produced.
Candidates can overlap; their nominal triangle savings must not be added.
"""

import argparse
import hashlib
import json
import math
import platform
import resource
import subprocess
import time
from collections import Counter
from fractions import Fraction as F
from pathlib import Path

from building_sample_originals import read_original
from experiment_output import create_output
from source_patch_fans import discover, retriangulate
from surface_patch_overlay import compare

ROOT = Path(__file__).resolve().parents[3]


def run(args):
    files = [
        Path(__file__).with_name(name)
        for name in (
            "source_patch_fan_study.py",
            "source_patch_fans.py",
            "surface_patch_overlay.py",
            "source_patch_candidates.py",
            "building_sample_originals.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured sources first")
    create_output(args.output)
    source, provenance = read_original(
        ROOT / "test/outshine/data/render/building-depth-samples/original-geometry",
        attributes=True,
    )
    start = time.process_time()
    fans, fields, normals, ids = discover(source)
    discovery_cpu = time.process_time() - start
    start = time.process_time()
    results, failures = [], Counter()
    maximum = F(0)
    worst = None
    for fan in fans:
        try:
            old, new, axes = retriangulate(source, fan, fields, normals, ids)
            result = compare(old, new, axes=axes)
        except ValueError as error:
            failures[str(error)] += 1
            continue
        if result["position_error_squared"] > maximum:
            maximum = result["position_error_squared"]
            worst = {
                "original_triangle_ordinals": fan["triangle_ordinals"],
                "part": fan["part"],
                "axes": axes,
                "positions": old[0].tolist(),
                "fields": old[1].tolist(),
                "original_faces": old[2].tolist(),
                "replacement_faces": new[2].tolist(),
                "result": result,
            }
        results.append(
            {
                **fan,
                **result,
                "proposed_triangles": len(new[2]),
                "local_numeric_copy_bytes": old[0].nbytes
                + old[1].nbytes
                + old[2].nbytes
                + new[2].nbytes,
            }
        )
    comparison_cpu = time.process_time() - start
    bins = {
        str(F(1, 10**power)): sum(
            row["position_error_squared"] <= F(1, 10**power) ** 2 for row in results
        )
        for power in range(1, 7)
    }
    summary = {
        "original_fans": len(fans),
        "fan_size_counts": dict(Counter(len(f["triangle_ordinals"]) for f in fans)),
        "complete_chart_comparisons": len(results),
        "fallback_reasons": dict(failures),
        "exact_zero_geometry": sum(
            row["position_error_squared"] == 0 for row in results
        ),
        "exact_zero_linear_fields": sum(
            not any(row["field_error_absolute"]) for row in results
        ),
        "diagnostic_geometry_distance_at_most_m": bins,
        "maximum_corresponding_point_distance_m": math.sqrt(float(maximum)),
        "total_triangle_pairs": sum(row["triangle_pairs"] for row in results),
        "total_overlay_vertex_evaluations": sum(
            row["overlay_vertex_evaluations"] for row in results
        ),
        "single_discovery_cpu_s": discovery_cpu,
        "single_compare_cpu_s": comparison_cpu,
        "process_peak_rss_platform_units": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss,
        "rss_scope": "Whole Python process incl input load; Linux KiB, macOS bytes. No incremental/UMA/device-memory claim.",
    }
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "platform": platform.platform(),
        "provenance": provenance,
        "summary": summary,
        "results": results,
        "scope": __doc__,
    }
    for name, data in (("report.json", report), ("worst.json", worst)):
        (args.output / name).write_text(json.dumps(data, default=str, indent=2) + "\n")
    print(json.dumps(summary))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
