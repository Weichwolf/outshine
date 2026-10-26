"""Matched-area terminal-shoot study using physical leaf meshes and union coverage."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from biome_models import plant_mesh
from broadleaf_crowns import architecture, match_area
from outshine_ref.vegetation.shoot_plan import normal_frame
from raster_batch import fragments
from scipy.spatial import ConvexHull


def leaf_geometry(mesh):
    faces = mesh["faces"][mesh["surface"] > 0]
    used, inverse = np.unique(faces, return_inverse=True)
    return mesh["vertices"][used], inverse.reshape(-1, 3)


def coverage(vertices, faces, lower, upper, size):
    pixels = np.zeros(size * size, dtype=bool)
    for _, _, index, _ in fragments(vertices, faces, lower, upper, size):
        pixels[index] = True
    area = float(np.prod(upper - lower) * pixels.sum() / (size * size))
    hull = float(ConvexHull(vertices[:, :2]).volume)
    return {
        "pixels": size,
        "leaf_union_m2": area,
        "convex_hull_m2": hull,
        "hull_fill": area / hull,
    }


def views(pair):
    result = []
    for direction in (
        (0, -18, 8.5),
        (0, 0, 1),
        (1, 0, 0),
        (0, 1, 0),
        (1, 1, 1),
        (-1, 1, 0.5),
    ):
        frame = normal_frame(np.array(direction, float), np.array((0, 0, 1)))
        basis = frame[:, (0, 2, 1)]
        projected = {name: (v @ basis, f) for name, (v, f) in pair.items()}
        both = np.concatenate([v for v, _ in projected.values()])[:, :2]
        center = (both.min(0) + both.max(0)) / 2
        half = np.ptp(both, axis=0).max() * 0.525
        lower, upper = center - half, center + half
        values = {
            name: [coverage(v, f, lower, upper, n) for n in (256, 512)]
            for name, (v, f) in projected.items()
        }
        result.append({"direction": direction, "models": values})
    return result


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for key in ("oak", "beech"):
        for target in (18, 55):
            terminal = match_area(architecture(key, 718, True), target)
            density = match_area(
                architecture(key, 718), terminal.summary()["leaf_area_m2"]
            )
            products, report = {}, {}
            for label, plan in (("density", density), ("terminal", terminal)):
                start = time.perf_counter()
                mesh = plant_mesh(plan, leaf_segments=8)
                elapsed = time.perf_counter() - start
                name = f"{key}-{target}-{label}"
                np.savez_compressed(output / (name + ".npz"), **mesh)
                (output / (name + "-plan.json")).write_text(
                    json.dumps(plan.recipe(), indent=2) + "\n"
                )
                summary = plan.summary()
                vertices, faces = leaf_geometry(mesh)
                p = vertices[faces]
                area = (
                    np.linalg.norm(
                        np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1
                    ).sum()
                    / 2
                )
                report[label] = {
                    **summary,
                    "mesh_s": elapsed,
                    "mesh_triangles": len(mesh["faces"]),
                    "mesh_leaf_area_m2": float(area),
                    "payload_bytes": sum(v.nbytes for v in mesh.values()),
                }
                products[label] = vertices, faces
                print(
                    json.dumps(
                        {
                            "case": name,
                            "leaf_area_m2": summary["leaf_area_m2"],
                            "mesh_s": elapsed,
                        }
                    ),
                    flush=True,
                )
            comparison = views(products)
            rows.append(
                {
                    "form": key,
                    "target_area_m2": target,
                    "models": report,
                    "views": comparison,
                }
            )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cases": rows,
        "scope": "Authored broadleaf comparison, not species or runtime admission. Both methods share lower woody axes and blade recipes; actual physical areas are reported. Pixel-center two-sided union of8-segment physical leaf meshes at256/512, six fixed views. Convex hull is a measurement proxy, not a botanical canopy or global coverage bound.",
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")


def cache_costs(output):
    """Decode complete prepared arrays once; OS cache state is not controlled."""
    rows = []
    for path in sorted(output.glob("*-*-*.npz")):
        start = time.perf_counter()
        with np.load(path) as stored:
            arrays = {key: stored[key] for key in stored.files}
        rows.append(
            {
                "file": path.name,
                "decode_s": time.perf_counter() - start,
                "compressed_bytes": path.stat().st_size,
                "returned_arrays": len(arrays),
                "returned_payload_bytes": sum(v.nbytes for v in arrays.values()),
            }
        )
        del arrays
    (output / "cache-costs.json").write_text(
        json.dumps({"scope": cache_costs.__doc__, "products": rows}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache-only", action="store_true")
    args = parser.parse_args()
    if args.cache_only:
        cache_costs(args.output)
    else:
        run(args.output)
