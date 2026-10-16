"""Actual point-upload work and traced placement memory, separate from image rendering."""

import argparse
import json
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import bpy
from blender_instances import instance_group
from render_stand_prototype import scene_population
from stand_prototype_scene import build_template, place_template


def measure(inputs, case, mode, output):
    _, origins, frames, scales, _, digest = scene_population(case, Path("."))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    uploaded = []

    def counted(prototype, positions, *args, **kwargs):
        uploaded.append(len(positions))
        return instance_group(prototype, positions, *args, **kwargs)

    start = time.perf_counter()
    with patch("stand_prototype_scene.instance_group", new=counted):
        template = build_template(inputs, mode)
        prepared = time.perf_counter()
        prototype_groups = len(uploaded)
        tracemalloc.start()
        report = place_template(template, origins, frames, scales, mode)
        placed = time.perf_counter()
        live, peak = tracemalloc.get_traced_memory()
        snapshot = tracemalloc.take_snapshot()
        tracemalloc.stop()
    assert sum(uploaded) == report["uploaded_point_rows"]
    point_rows = sum(uploaded)
    result = {
        "case": case,
        "mode": mode,
        "population_sha256": digest,
        "prototype_s_untraced": prepared - start,
        "placement_s_traced": placed - prepared,
        "placement_live_traced_bytes": live,
        "placement_peak_traced_bytes": peak,
        "placement_live_traced_blocks": sum(
            s.count for s in snapshot.statistics("filename")
        ),
        "point_group_calls": len(uploaded),
        "prototype_point_group_calls": prototype_groups,
        "point_rows_written": point_rows,
        "explicit_float32_staging_arrays": len(uploaded) * 3,
        "explicit_float32_staging_bytes": point_rows * 36,
        "explicit_point_attribute_write_bytes": point_rows * 36,
        "float64_euler_result_bytes": point_rows * 24,
        "plant_report": report,
        "scope": "Actual constructor calls and known staging/writes, not total allocations/copied bytes. Traced placement excludes prior template and Blender/native/driver heaps. Timing instrumentation differs from rendering; no GPU/frame claim.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument(
        "--case", choices=("pair", "Laubwald", "Mischwald"), required=True
    )
    parser.add_argument("--mode", choices=("direct", "nested", "packed"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    measure(args.inputs, args.case, args.mode, args.output)
