"""Explain native coverage stages and test one source-depth-fitted support plane."""

import argparse
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from building_plane_coverage import capture_points, geometry_controls, signed_components
from building_sample_inputs import (
    capture_fields,
    crop_view,
    digest,
    readbacks,
    same_sources,
)
from building_sample_originals import read_original
from building_sample_source import read_query, read_source
from compare_building_sample_reuse import DATA, errors
from field_plane_integrals import cell_integrals, mask_integrals
from field_view_integrals import summed_field
from field_view_plane import FieldPlane, fit_plane
from field_view_sampling import reconstruct
from process_memory import peak_rss_bytes


def image(field, grid, view, plane):
    result = reconstruct(field, grid, view, plane=plane, approximate=True)
    values = np.zeros((view.height, view.width, 4))
    x, y = result["origin"]
    h, w = result["rgba"].shape[:2]
    values[y : y + h, x : x + w] = result.pop("rgba")
    return values, result


def analyse(source, primary, high, canonical, original, depth_query):
    manifest, raw, grid, high_view = source
    same_sources(primary[0], high[0])
    same_sources(primary[0], canonical[0])
    plane, fit = fit_plane(raw["source_depth"], raw["source_surface"] > 0)
    fit.update(
        coefficients=plane.coefficients,
        world_normal=plane.normal(grid).tolist(),
        rms_m=fit["normalized_depth_rms"] * grid.half_extent_m,
    )
    field = summed_field(raw["source_lit_rgba"], dtype=np.float64)
    images = {}
    rows = []
    poses = {}
    for group, (data, arrays) in (("primary", primary), ("high", high)):
        for index, case in enumerate(data["cases"]):
            query = case["query"]
            view = crop_view(data, query)
            if query["sun_elevation_deg"] != manifest["query"]["sun_elevation_deg"]:
                raise ValueError(
                    "Fixed capture lighting required for a plane-only comparison"
                )
            key = json.dumps(
                {k: query[k] for k in ("eye", "basis", "focal_px")}, sort_keys=True
            )
            if key not in poses:
                poses[key] = {
                    name: image(field, grid, view, support)
                    for name, support in (("flat", None), ("fitted", plane))
                }
            reference = arrays[
                f"case{index}_original_ss{query['filter_axis_samples'] ** 2}"
            ]
            row = {"query": query, "reference_set": group, "variants": {}}
            images[f"{group}{index}_original"] = reference
            for name, (values, sampling) in poses[key].items():
                images[f"{group}{index}_{name}"] = values
                row["variants"][name] = {
                    "original_vs_candidate": errors(reference, values),
                    "sampling": sampling,
                }
            rows.append(row)
    geometry, shapes, geometry_work = geometry_controls(
        original, grid, high_view, plane
    )
    images.update(geometry)
    ideal, calibration = capture_points(
        shapes["flat"], grid, high_view, raw["source_surface"] > 0
    )
    canonical_grid, canonical_fields = capture_fields(*canonical)
    if (
        not np.allclose(canonical_grid.centre, grid.centre, rtol=0, atol=1e-9)
        or canonical_grid.toward_eye != grid.toward_eye
        or canonical_grid.half_extent_m != grid.half_extent_m
    ):
        raise ValueError("Original128 and512 capture grids differ")
    _, calibration128 = capture_points(
        shapes["flat"], grid, high_view, canonical_fields["samples"][:, :, 3] > 0
    )
    coverage, stages = signed_components(
        high[1]["case0_original_ss4096"],
        raw["prefix_linear"],
        images["high0_flat"],
        ideal,
        geometry,
        grid,
        high_view,
    )
    images.update({"stage_" + k: v for k, v in stages.items()})
    images["ideal_source_mask512"] = ideal
    controls = {}
    for name, support in (("flat", FieldPlane()), ("fitted", plane)):
        reference, work = cell_integrals(
            raw["source_lit_rgba"], grid, high_view, support
        )
        mask, mask_work = mask_integrals(
            raw["source_lit_rgba"], grid, high_view, support
        )
        images["area_" + name] = reference
        controls[name] = {
            "original_vs_projected_cell_field": errors(
                high[1]["case0_original_ss4096"], reference
            ),
            "projected_cell_field_vs_prefix": errors(
                reference, images["high0_" + name]
            ),
            "independent_union_alpha_max_error": float(
                abs(mask - reference[:, :, 3]).max()
            ),
            "cell_work": work,
            "mask_work": mask_work,
        }
    images["native_depth_ss64"] = depth_query[1]["ss4096"]
    images["native_flat_linear"] = raw["prefix_linear"]
    return {
        "fit": fit,
        "rows": rows,
        "unique_poses": len(poses),
        "source_pixels": 512,
        "prefix_bytes": field.table.nbytes,
        "prefix_constructions": 1,
        "geometry_work": geometry_work,
        "coverage_components": coverage,
        "capture_point_controls": [calibration, calibration128],
        "projected_cell_controls": controls,
        "native_original_vs_depth_preserving_samples": errors(
            high[1]["case0_original_ss4096"], depth_query[1]["ss4096"]
        ),
        "native_depth_samples_vs_flat_prefix": errors(
            depth_query[1]["ss4096"], raw["prefix_linear"]
        ),
        "scope": "Same complete1558-source original group/frames/lighting and full1280x720 rays. One plane fitted from every first-hit depth, never RGB/query pose; same coupled source field and one prefix. Held-capture PBR on other poses, finite native references and independent numerical GEOS field areas declared separately. No original shading reconstruction, new renderer/representation family, native change, general pose/LOD/World/main-/shadowdepth/p99/UMA admission.",
    }, images


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp output required")
    names = (
        "building_plane_coverage.py",
        "building_sample_inputs.py",
        "building_sample_originals.py",
        "building_sample_source.py",
        "field_plane_integrals.py",
        "field_view_integrals.py",
        "field_view_sampling.py",
        "field_view_plane.py",
        "triangle_coverage_reference.py",
        "process_memory.py",
    )
    tracked = [Path(__file__), *(Path(__file__).with_name(n) for n in names)]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, tracked)]
    ):
        raise ValueError(
            "Freeze actual source/plane/integration/reference inputs first"
        )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    start, cpu, load = time.perf_counter(), time.process_time(), os.getloadavg()
    source = read_source(args.inputs / "source-512", args.inputs / "high-sampling")
    primary, high, canonical = (
        readbacks(p)
        for p in (args.inputs, args.inputs / "high-sampling", args.inputs / "same-rays")
    )
    original, audit = read_original(args.inputs / "original-geometry")
    depth_query = read_query(
        args.inputs / "samples-512",
        args.inputs / "source-512",
        args.inputs / "high-sampling",
    )
    decoded_wall, decoded_cpu = time.perf_counter() - start, time.process_time() - cpu
    report, images = analyse(source, primary, high, canonical, original, depth_query)
    report.update(
        commit=commit,
        profile=[1280, 720],
        original_geometry=audit,
        input_manifests={
            str(p): digest(p / "manifest.json")
            for p in (
                args.inputs,
                args.inputs / "source-512",
                args.inputs / "samples-512",
                args.inputs / "high-sampling",
                args.inputs / "same-rays",
                args.inputs / "original-geometry",
            )
        },
        cost={
            "verify_decode_wall_s": decoded_wall,
            "verify_decode_cpu_s": decoded_cpu,
            "analysis_wall_s": time.perf_counter() - start - decoded_wall,
            "analysis_cpu_s": time.process_time() - cpu - decoded_cpu,
            "peak_process_rss_bytes": peak_rss_bytes(),
            "held_output_array_bytes": sum(v.nbytes for v in images.values()),
            "nice": os.getpriority(os.PRIO_PROCESS, 0),
            "loadavg_before": load,
            "platform": platform.platform(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "shapely": shapely.__version__,
            "scope": "Diagnostic fit/eight plane samplings, independent source-cell/GEOS/original comparisons. Not native/device times; internal allocation/total-copy traffic unmeasured, held bytes not added to OS peak.",
        },
    )
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output / "plane-controls.npz", **images)
    report["controls_sha256"] = digest(args.output / "plane-controls.npz")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": commit, "output": str(args.output)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
