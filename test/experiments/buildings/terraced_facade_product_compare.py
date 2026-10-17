"""Compare the discarded polygonal prototype with identical batched facade coverage."""

import argparse
import importlib.util
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
import shapely

from terraced_facade_prepare import prepare
from terraced_facade_surface import compact
from terraced_mass_plan import plan
from terraced_mass_sources import sources
from evidence import revision


def reference(path):
    spec = importlib.util.spec_from_file_location("compact_polygonal_reference", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def coverage(product, prepared):
    core = prepared.mass.core
    across, along = np.asarray(core.across_xy), np.asarray(core.along_xy)
    frames = {}
    for w in prepared.facade.windows:
        centre = np.asarray(core.origin_xy) + w.centre[0] * across + w.centre[1] * along
        tangent = w.tangent[0] * across + w.tangent[1] * along
        frames[w.identifier] = centre, tangent, w.bottom_m
    groups = {}
    xyz = product.mesh["vertices"][product.mesh["faces"]]
    for triangle, material, owner in zip(
        xyz, product.mesh["materials"], product.triangle_owners, strict=True
    ):
        name = product.owner_ids[int(owner)]
        if name not in frames:
            continue
        centre, tangent, bottom = frames[name]
        normal = np.array([tangent[1], -tangent[0]])
        xy = triangle[:, :2] - centre
        depth = xy @ normal
        if np.ptp(depth) > 1e-8:
            raise ValueError("Compact window escaped its declared metrical plane")
        key = name, int(material), round(float(depth.mean()), 10)
        points = np.c_[xy @ tangent, triangle[:, 2] - bottom]
        groups.setdefault(key, []).append(shapely.Polygon(points))
    return {key: shapely.union_all(triangles) for key, triangles in groups.items()}


def compare(module):
    rows = []
    for row in sources():
        for kind in ("cascade", "paired-wings"):
            prepared = prepare(plan(row, kind))
            before, after = module.compact(prepared), compact(prepared)
            left, right = coverage(before, prepared), coverage(after, prepared)
            if left.keys() != right.keys():
                raise ValueError(
                    "Batched compact window/material/depth ownership changed"
                )
            errors = [left[k].symmetric_difference(right[k]).area for k in left]
            if max(errors, default=0) > 1e-8:
                raise ValueError("Batched compact frame/glass/sill coverage changed")
            if before.window_ids != after.window_ids:
                raise ValueError("Batched compact logical windows changed")
            rows.append(
                {
                    "source_id": row["id"],
                    "programme": kind,
                    "window_material_planes": len(left),
                    "max_coverage_xor_m2": max(errors, default=0),
                    "sum_coverage_xor_m2": sum(errors),
                    "triangles_before": len(before.mesh["faces"]),
                    "triangles_after": len(after.mesh["faces"]),
                }
            )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-module", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    result = compare(reference(args.reference_module))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(
            revision()
            | {
                "records": result,
                "reference_module_sha256": hashlib.sha256(
                    args.reference_module.read_bytes()
                ).hexdigest(),
            },
            indent=2,
        )
        + "\n"
    )
    print(
        json.dumps(
            {
                "sources": len(result),
                "max_xor_m2": max(r["max_coverage_xor_m2"] for r in result),
            }
        )
    )


if __name__ == "__main__":
    main()
