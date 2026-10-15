"""Compare cached pinna alternatives and prepare bounded existing-renderer inputs."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from fern_mesh import fern_mesh
from pinna_boundary import (
    boundary_product,
    correspondence,
    group_bounds,
    select,
    snapshot,
)
from pinna_product import group_mask, prepare
from pinna_surface import overlay, surface
from scipy.spatial.transform import Rotation


def light_requests(receiver_radius_m):
    sun = Rotation.from_euler("xyz", (0.45, -0.65, -0.6)).apply((0, 0, 1))
    area = np.asarray((-8, -12, 18), float)
    area /= np.linalg.norm(area)
    # Bound the entire emitting disk and all receivers by spheres, rather
    # than treating every root and leaf as the tile centre.
    separation = np.linalg.norm((-8, -12, 18))
    radius = np.arcsin((9 + receiver_radius_m) / separation)
    angles = np.array((0, 0.04, radius))
    directions = np.array(((4, -6, 3.82), sun, area), float)
    directions /= np.linalg.norm(directions, axis=1, keepdims=True)
    cosine = directions[:, 2]
    minimum = cosine * np.cos(angles) - np.sqrt(1 - cosine**2) * np.sin(angles)
    gains = np.r_[1, 1 / minimum[1:]]
    return directions, angles, gains


def compare(plan, source, full, scale, pixels_per_metre, receiver_radius_m):
    directions, angles, gains = light_requests(receiver_radius_m)
    rows = []
    for method in ("quad", "contour"):
        start = time.perf_counter()
        coarse, report = surface(source, method)
        transfer = overlay(source, coarse)
        mapped, records = prepare(plan, source, coarse, transfer)
        selected = group_mask(
            records,
            mapped,
            directions,
            scale,
            pixels_per_metre,
            angular_radii=angles,
            projection_gains=gains,
        )
        rows.append(
            report
            | {
                "preparation_s": time.perf_counter() - start,
                "canonical_faces": len(coarse["faces"]),
                "selected_groups": int(selected.sum()),
                "remaining_plant_triangles": len(full["faces"])
                - int(selected.sum())
                * (report["source_leaf_faces"] - len(coarse["faces"])),
                "overlay_points": transfer["points"],
                "atlas_scope": "No textured render or atlas consumer admitted; geometry/selection prefilter only.",
            }
        )
    return rows


def run(inputs, output, span_m):
    if not 1 <= span_m <= 32 or output.resolve() == inputs.resolve():
        raise ValueError("separate output and bounded orthographic width required")
    output.mkdir(parents=True, exist_ok=False)
    for path in inputs.iterdir():
        if path.is_file():
            (output / path.name).symlink_to(path.resolve())
    poses = dict(np.load(inputs / "fern-population.npz"))
    receiver_radius_m, grass_triangles = 0.0, 0
    for kind, count in (("fern", 4), ("grass", 8)):
        population = dict(np.load(inputs / (kind + "-population.npz")))
        for variant in range(count):
            mask = population["variants"] == variant
            if not mask.any():
                continue
            suffix = "-field" if kind == "fern" else ""
            mesh = dict(np.load(inputs / f"{kind}-{variant}{suffix}.npz"))
            extent = np.linalg.norm(mesh["vertices"], axis=1).max()
            radii = np.linalg.norm(population["origins"][mask], axis=1)
            scales = population["scales"][mask]
            if scales.ndim == 2:
                scales = scales.max(axis=1)
            receiver_radius_m = max(
                receiver_radius_m, float((radii + scales * extent).max())
            )
            if kind == "grass":
                grass_triangles += int(mask.sum()) * len(mesh["faces"])
    gain = float(light_requests(receiver_radius_m)[2].max())
    pixels_per_metre = 1920 / span_m
    rows, total_before, total_after = [], 0, 0
    for variant in range(4):
        plan = json.loads((inputs / f"fern-{variant}-plan.json").read_text())
        source = dict(np.load(inputs / f"pinna-{variant}-field.npz"))
        full = dict(np.load(inputs / f"fern-{variant}-field.npz"))
        root_scales = poses["scales"][poses["variants"] == variant]
        scale = float(root_scales.max())
        start = time.perf_counter()
        coarse = boundary_product(source)
        transfer = correspondence(source, coarse)
        canonical_s = time.perf_counter() - start
        start = time.perf_counter()
        mapped = fern_mesh(plan, coarse, rib_segments=10)
        assembly_s = time.perf_counter() - start
        start = time.perf_counter()
        records = group_bounds(full, mapped, transfer)
        bounds_s = time.perf_counter() - start
        start = time.perf_counter()
        chosen = select(records, scale, pixels_per_metre, gain)
        lookup_s = time.perf_counter() - start
        product = snapshot(full, mapped, chosen)
        path = output / f"fern-{variant}-field.npz"
        path.unlink()
        np.savez_compressed(path, **product)
        before, after = len(full["faces"]), len(product["faces"])
        total_before += before * len(root_scales)
        total_after += after * len(root_scales)
        row = {
            "variant": variant,
            "roots": len(root_scales),
            "maximum_root_scale": scale,
            "source_triangles": before,
            "candidate_triangles": len(mapped["faces"]),
            "selected_triangles": after,
            "selected_groups": len(chosen),
            "candidate_payload_bytes": sum(a.nbytes for a in mapped.values()),
            "source_payload_bytes": sum(a.nbytes for a in full.values()),
            "selected_payload_bytes": sum(a.nbytes for a in product.values()),
            "canonical_s": canonical_s,
            "assembly_s": assembly_s,
            "bounds_s": bounds_s,
            "lookup_s": lookup_s,
            "records": records,
        }
        if variant == 0:
            row["plane_prefilter"] = compare(
                plan, source, full, scale, pixels_per_metre, receiver_radius_m
            )
        rows.append(row)
    result = {
        "orthographic_width_m": span_m,
        "resolution": [1920, 1080],
        "max_surface_displacement_px": 0.75,
        "maximum_flat_ground_shadow_gain": gain,
        "receiver_sphere_radius_m": receiver_radius_m,
        "fern_instance_triangles_before": total_before,
        "fern_instance_triangles_after": total_after,
        "unchanged_grass_triangles": grass_triangles,
        "variants": rows,
        "scope": "Finite cached surfaces; orthographic camera and flat-ground direct-light displacement only. No coverage/radiance/general-shadow/LOD/native acceptance. Existing renderer uses the selected complete cache products.",
    }
    (output / "pinna-comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "variants"}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--span-m", type=float, default=7.4)
    args = parser.parse_args()
    run(args.inputs, args.output, args.span_m)
