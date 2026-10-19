"""Fit shared physical Ill facets from the original inputs, with declared controls."""

import argparse
import hashlib
import json
import platform
import tempfile
import time
import tracemalloc
from pathlib import Path

import clarabel
import numpy as np
import pyproj
import scipy
import shapely
from compare_native_water_frame import course_controls, footprints, original_case
from directional_river_fit import infer_flow, source_face_rows
from geodetic_facet import local_points
from outshine_ref.contracts import Mesh
from physical_river_fit import fit_physical_surface
from physical_river_frame import (
    inverse_frame,
    model_height_targets,
    normal_caps,
    tangent_directions,
)
from river_membership import face_membership
from river_profiles import RiverProfile
from river_surface import surface_mesh


def inputs(manifest_path):
    data, manifest, legacy = original_case(manifest_path)
    anchor = np.array(legacy["frame"]["origin_lon_lat"])
    selection = manifest["original_selection"]
    original, polygons = footprints(data, selection["selected_surface_indices"], anchor)
    flat, topology = surface_mesh(shapely.union_all(polygons), 20)
    csr = face_membership(flat.vertices, flat.triangles, polygons)
    if np.any(csr.status):
        raise ValueError("complete original source membership required")
    courses, course_report = course_controls(
        data, selection["selected_course_indices"], anchor, np.ones(2)
    )
    geographic = courses["course_original_lon_lat"]
    placed = local_points(anchor, geographic.reshape(-1, 2), 0).reshape(-1, 2, 3)
    segments = placed[:, :, :2]
    levels = courses["course_original_levels_m"]
    target, references = RiverProfile(segments, levels).sample(
        flat.vertices[:, :2], "blend-affine"
    )
    if not np.isfinite(target).all():
        raise ValueError("declared prior sampling reference budget exhausted")
    up_target = model_height_targets(anchor, flat.vertices[:, :2], target)
    mesh = Mesh(np.c_[flat.vertices[:, :2], up_target], flat.triangles)
    _flow, nearest, distance = infer_flow(mesh, segments, levels)
    old_rows, source, coverage = source_face_rows(mesh, segments, levels)
    direction = np.diff(placed, axis=1)[:, 0]
    direction *= -np.sign(np.diff(levels, axis=1)[:, 0, None])
    # The finite fixture's original positive levels allow this explicit exterior prism.
    box = [float(up_target.min() - 20), float(up_target.max() + 20)]
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, box)
    along = tangent_directions(caps["up"], direction[nearest])
    cross = np.cross(caps["up"], along)
    faces = np.arange(len(mesh.triangles))
    rows = np.concatenate(
        [
            np.c_[
                faces,
                direction[nearest],
                np.full(len(faces), -0.1),
                np.zeros(len(faces)),
            ],
            np.c_[
                faces, cross, np.full(len(faces), -0.005), np.full(len(faces), 0.005)
            ],
            np.c_[old_rows[:, 0], direction[source], old_rows[:, 3:5]],
        ]
    )
    arrays = {
        "original_fragment_triangles_enu_m": original["fragment_triangles_enu_m"],
        "original_fragment_surface_index": original["fragment_surface_index"],
        "original_surface_level_prior_m": data["surfaces"][
            selection["selected_surface_indices"]
        ]["level_m"].astype(float),
        "target_geodetic_model_height_m": target,
        "target_enu_m": mesh.vertices,
        "triangles": mesh.triangles,
        "rows": rows,
        "nearest_course_segment": nearest,
        "source_face_segment": source,
        "source_offsets": csr.offsets,
        "source_members": csr.members,
        "source_representative": csr.representative,
        "source_original_indices": np.array(selection["selected_surface_indices"]),
        "course_original_address": courses["course_original_address"],
        "course_original_lon_lat": geographic,
        "course_original_levels_m": levels,
        "cap_delta": caps["delta"],
        "cap_up": caps["up"],
        "row_tangent": tangent_directions(
            caps["up"][rows[:, 0].astype(int)], rows[:, 1:4]
        ),
    }
    report = {
        "topology": topology,
        "membership": csr.work,
        "directions": coverage,
        "original_course_records": course_report["original_segments"],
        "prior_enumerated_references": int(references.sum()),
        "nearest_course_distance_max_m": float(distance.max()),
        "up_bounds_m": box,
        "source_manifest": str(manifest_path),
        "historical_sha256": manifest["historical_v2"]["sha256"],
        "fixture_sha256": hashlib.sha256(
            (manifest_path.parent / manifest["fixture"]).read_bytes()
        ).hexdigest(),
        "source_observation": "later replay metadata/permutation remain in the hash-pinned original fixture; not a historical epoch or flow-tag claim",
        "prior_policy": "original DEM-derived course levels form a soft blend-affine target; coarse flat surface percentiles retained as provenance/baseline, not imposed as constant heights",
        "hard_conditions": "shared vertices, fixed reference XY coverage/island, declared physical facet grade intervals and explicit ENU-Up contacts; no observed height pins in the real case",
        "direction_scope": "height-delta hypotheses, not observed source flow directions",
        "outside_selection": "original full WATER2 retained; finite selection unchanged",
    }
    levels_flat = arrays["original_surface_level_prior_m"]
    conflicts = [
        (abs(levels_flat[i] - levels_flat[j]), polygons[i].intersection(polygons[j]))
        for i in range(len(polygons))
        for j in range(i + 1, len(polygons))
        if polygons[i].intersection(polygons[j]).area > 0
    ]
    gap, intersection = max(conflicts, key=lambda item: item[0])
    arrays["diagnostic_crop_xy"] = np.asarray(intersection.centroid.coords)[0]
    report["diagnostic_old_prior_gap_m"] = float(gap)
    return mesh, anchor, rows, box, arrays, report


def sampled_grades(mesh, anchor, samples_per_edge=8):
    """Independent actual-facet samples; cap proof is separate, samples cannot replace it."""
    bary = (
        np.array(
            [
                [i, j, samples_per_edge - i - j]
                for i in range(samples_per_edge + 1)
                for j in range(samples_per_edge + 1 - i)
            ],
            float,
        )
        / samples_per_edge
    )
    points = np.einsum("ki,fij->fkj", bary, mesh.vertices[mesh.triangles])
    geographic, heights, up = inverse_frame(anchor, points.reshape(-1, 3))
    up = up.reshape(len(mesh.triangles), len(bary), 3)
    triangle = mesh.vertices[mesh.triangles]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    return bary, geographic, heights, up, normal


def audit(mesh, anchor, rows, box, row_tangent):
    bary, geographic, heights, up, normal = sampled_grades(mesh, anchor)
    faces = rows[:, 0].astype(int)
    selected_up = up[faces]
    projected = (
        row_tangent[:, None]
        - np.sum(row_tangent[:, None] * selected_up, axis=2)[:, :, None] * selected_up
    )
    projected /= np.linalg.norm(projected, axis=2)[:, :, None]
    numerator = -np.sum(normal[faces, None] * projected, axis=2)
    denominator = np.sum(normal[faces, None] * selected_up, axis=2)
    grades = numerator / denominator
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, box)
    delta = np.linalg.norm(up - caps["up"][:, None], axis=2)
    return {
        "barycentric_samples_per_face": len(bary),
        "sampled_points": len(geographic),
        "sampled_directed_grades": int(grades.size),
        "sampled_upper_bound_violating_rows": int(
            np.count_nonzero(grades.max(1) > rows[:, 5] + 1e-9)
        ),
        "sampled_lower_bound_violating_rows": int(
            np.count_nonzero(grades.min(1) < rows[:, 4] - 1e-9)
        ),
        "sampled_violating_rows": int(
            np.count_nonzero(
                (grades.max(1) > rows[:, 5] + 1e-9)
                | (grades.min(1) < rows[:, 4] - 1e-9)
            )
        ),
        "sampled_physical_grade_violation": max(
            float((grades - rows[:, 5, None]).max()),
            float((rows[:, 4, None] - grades).max()),
            0.0,
        ),
        "sampled_cap_excess": float((delta - caps["delta"][:, None]).max()),
        "declared_model_height_range_m": [float(heights.min()), float(heights.max())],
    }, {
        "sample_geographic_lon_lat": geographic,
        "sample_geodetic_height_m": heights,
        "sample_grade_minmax": np.c_[grades.min(1), grades.max(1)],
        "sample_cap_distances": delta,
    }


def run(manifest_path, output):
    began = time.perf_counter()
    tracemalloc.start()
    mesh, anchor, rows, box, arrays, report = inputs(manifest_path)
    report["input_python_traced_peak_bytes"] = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    report["input_wall_seconds"] = time.perf_counter() - began
    report["methods"] = {}
    for method in ("enu-control", "centre", "whole-prism"):
        tracemalloc.start()
        fit = fit_physical_surface(
            mesh, anchor, rows, up_bounds_m=box, normal_model=method
        )
        result = dict(fit.report)
        result["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()[1]
        tracemalloc.stop()
        if fit.mesh is not None:
            sampled, captured = audit(
                fit.mesh, anchor, rows, box, arrays["row_tangent"]
            )
            result.update(sampled)
            arrays[method + "_vertices"] = fit.mesh.vertices
            arrays.update({method + "_" + k: v for k, v in captured.items()})
        report["methods"][method] = result
    report["numeric_wall_seconds"] = time.perf_counter() - began
    report["retained_array_bytes"] = sum(x.nbytes for x in arrays.values())
    report["environment"] = {
        "system": platform.system(),
        "machine": platform.machine(),
        "cpu": next(
            (
                line.split(":", 1)[1].strip()
                for line in Path("/proc/cpuinfo").read_text().splitlines()
                if line.startswith("model name")
            ),
            "unknown",
        )
        if Path("/proc/cpuinfo").exists()
        else platform.processor(),
        "numpy": np.__version__,
        "scipy": scipy.__version__,
        "pyproj": pyproj.__version__,
        "proj": pyproj.proj_version_str,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "clarabel": clarabel.__version__,
    }
    report["experiment_sources_sha256"] = {
        name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
        for name in (
            "physical_river_frame.py",
            "physical_river_fit.py",
            "physical_river_program.py",
            "compare_physical_river.py",
        )
    }
    report["cost_scope"] = (
        "instrumented Python preparation/solver wrapper; GEOS/PROJ/Clarabel native allocations, factorization work and total copied bytes not measured; no native runtime or device admission"
    )
    np.savez_compressed(output / "physical-fit.npz", **arrays)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return arrays, report


def plot(arrays, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(12.8, 8.0), layout="constrained")
    centre = arrays["diagnostic_crop_xy"]
    half = 180
    faces = arrays["triangles"]
    vertices = arrays.get("whole-prism_vertices", arrays["target_enu_m"])
    low, high = vertices[:, 2].min(), vertices[:, 2].max()
    ax = fig.add_subplot(2, 2, 1)
    image = ax.tripcolor(
        *vertices[:, :2].T,
        vertices[:, 2],
        triangles=faces,
        vmin=low,
        vmax=high,
        cmap="viridis",
    )
    ax.add_patch(
        Rectangle(centre - half, 2 * half, 2 * half, fill=False, edgecolor="red")
    )
    ax.set(
        title="Shared physical surface / zero-datum reference footprint",
        xlabel="ENU East [m]",
        ylabel="ENU North [m]",
        aspect="equal",
    )
    fig.colorbar(image, ax=ax, label="ENU Up [m]")
    ax = fig.add_subplot(2, 2, 2)
    if "enu-control_sample_grade_minmax" in arrays:
        values = arrays["enu-control_sample_grade_minmax"]
        rows = arrays["rows"]
        violation = np.maximum(rows[:, 4] - values[:, 0], values[:, 1] - rows[:, 5])
        per_face = np.zeros(len(faces))
        np.maximum.at(per_face, rows[:, 0].astype(int), violation)
        middle = vertices[faces, :2].mean(1)
        image = ax.scatter(*middle.T, c=per_face * 1e6, s=4, cmap="inferno")
        fig.colorbar(image, ax=ax, label="Sampled physical violation [µm/m]")
    ax.set(
        title="ENU control: physical grade error (whole-prism: zero)",
        xlabel="ENU East [m]",
        ylabel="ENU North [m]",
        aspect="equal",
    )
    triangles = [arrays["original_fragment_triangles_enu_m"], vertices[faces]]
    for index, (title, triangle) in enumerate(
        zip(
            ("Original DEM-percentile fragments", "Shared physical fit"),
            triangles,
            strict=True,
        )
    ):
        ax = fig.add_subplot(2, 2, index + 3, projection="3d")
        local = triangle.copy()
        local[:, :, :2] -= centre
        keep = (local[:, :, :2].min(1) <= half).all(1) & (
            local[:, :, :2].max(1) >= -half
        ).all(1)
        collection = Poly3DCollection(local[keep], edgecolors="none", cmap="viridis")
        collection.set_array(local[keep, :, 2].mean(1))
        collection.set_clim(low, high)
        ax.add_collection3d(collection)
        ax.set(
            xlim=(-half, half),
            ylim=(-half, half),
            zlim=(440, 470),
            title=title,
            xlabel="East from crop centre [m]",
            ylabel="North from crop centre [m]",
            zlabel="ENU Up [m]",
        )
        ax.view_init(elev=25, azim=-50)
    fig.suptitle(
        "Original Ill case: soft DEM priors, shared height DOFs, explicit physical grade bounds"
    )
    fig.savefig(output / "physical-fit.png", dpi=120)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp outside the checkout")
    output.mkdir(parents=True, exist_ok=False)
    arrays, report = run(args.input, output)
    plot(arrays, output)
    report["artifacts"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in output.iterdir()
        if p.suffix in (".npz", ".png")
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["methods"], indent=2))


if __name__ == "__main__":
    main()
