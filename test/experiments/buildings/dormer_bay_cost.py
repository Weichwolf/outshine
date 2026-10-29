"""Equal aligned selection methods and explicit whole-source architectural cost."""

import argparse
import gc
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from dormer_plan import plan
from dormer_sources import sources
from provider_contract import attributes
from tile_dormers import DormerFacades
from tile_roof_envelopes import choose
from verify_elevations import untimed
from verify_facade_frames import view_for


def build(row, neighbors, *, layout, method):
    owner = DormerFacades(
        [row, *neighbors], view_for(row), dormer_layout=layout, bay_method=method
    )
    source = owner.rows[0]
    mesh, info = owner(source, attributes(source["properties"]), 12)
    return mesh, info, owner


def same_plan(a, b):
    if a != b:
        raise ValueError("Complete aligned plans or rejected candidates differ")


def same_body(a, b):
    if a[0].keys() != b[0].keys() or untimed(a[1]) != untimed(b[1]):
        raise ValueError("Complete source mesh fields or logical ownership changed")
    for key, first in a[0].items():
        second = b[0][key]
        if (
            first.dtype != second.dtype
            or first.shape != second.shape
            or first.tobytes() != second.tobytes()
        ):
            raise ValueError("Complete source geometry changed: " + key)


def selected(method, source, asset, choice, entrances):
    return plan(
        source,
        asset.original,
        choice,
        frontages=asset.frontages,
        bay_method=method,
        clear=lambda geometry, low, high: entrances.clear(
            source, geometry, high, low_m=low
        ),
    )


def compare(methods, verify, output, prefix):
    names = tuple(methods)
    expected = {name: call() for name, call in methods.items()}
    if "linear" in expected:
        verify(expected["linear"], expected["bisect"])
    report = {name: {"rounds": []} for name in names}
    for iteration in range(9):
        order = names[iteration % len(names) :] + names[: iteration % len(names)]
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = methods[name]()
            report[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            verify(expected[name], value)
            del value
    for name in names:
        gc.collect()
        value, memory = allocations(output / f"{prefix}-{name}.bin", methods[name])
        verify(expected[name], value)
        report[name].update(memory)
        for clock in ("cpu", "wall"):
            report[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in report[name]["rounds"]])
            )
        del value
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Run cost comparisons at normal priority")
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        raise ValueError("Private cost output requires a fresh system-temp directory")
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", "test/experiments/buildings"],
        text=True,
    ):
        raise ValueError("Freeze roof/facade producers before recording costs")
    output.mkdir(parents=True, exist_ok=False)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "platform": platform.platform(),
        "priority": os.getpriority(os.PRIO_PROCESS, 0),
        "loadavg_start": os.getloadavg(),
        "total_internal_copied_bytes": None,
        "scope": "Nine rotating normal-priority complete queries; checks outside timing and separate Memray/native-stack allocation profiling. Plan-only borrows actual prepared roof/upper facade and source-neighbor clearance. Whole source includes fresh existing owner initialization, early source/facade/entry planning, roof/body/facade geometry and actual demanded near windows. Linear and bisect produce identical complete products. Edge default is an architectural cost reference with different geometry, not an equal-quality speedup comparison. Source loading/imports and rendering excluded. Existing input residency excluded from scoped heap peaks; no RSS/UMA/native frame/device claim.",
        "cases": [],
    }
    started = time.process_time()
    rows = sources()
    report["source_loading_cpu_s_single_run"] = time.process_time() - started
    for index, (row, neighbors) in enumerate(rows):
        prepared = build(row, neighbors, layout="facade", method="bisect")
        owner = prepared[2]
        source = owner.rows[0]
        params = attributes(source["properties"])
        choice = choose(source, params)
        asset = owner.base.dormer_assets[row["id"]]
        if not asset.frontages:
            raise ValueError("Cost case fell back without source facade axes")

        record = {
            "case": index,
            "source_id": row["id"],
            "upper_facades": len(asset.frontages),
            "upper_bay_coordinates": sum(f["columns"] for f in asset.frontages),
            "admitted_dormers": len(asset.dormers),
            "rejected_candidates": asset.rejected,
            "guaranteed_frame_coordinate_materialization_bytes": len(asset.frontages)
            * 32,
            "output_array_bytes": sum(a.nbytes for a in prepared[0].values()),
            "output_array_count": len(prepared[0]),
            "selected_near": asset.near is not None,
            "plan": compare(
                {
                    name: lambda name=name, source=source, asset=asset, choice=choice, entrances=owner.entrances: (
                        selected(name, source, asset, choice, entrances)
                    )
                    for name in ("linear", "bisect")
                },
                same_plan,
                output,
                f"plan-{index}",
            ),
            "whole_source": compare(
                {
                    name: lambda name=name, row=row, neighbors=neighbors: build(
                        row,
                        neighbors,
                        layout="edge" if name == "edge" else "facade",
                        method="bisect" if name == "edge" else name,
                    )
                    for name in ("edge", "linear", "bisect")
                },
                same_body,
                output,
                f"whole-{index}",
            ),
        }
        report["cases"].append(record)
        print(
            json.dumps({"case": index, "state": "complete_equal_aligned_products"}),
            flush=True,
        )
    report["loadavg_end"] = os.getloadavg()
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
