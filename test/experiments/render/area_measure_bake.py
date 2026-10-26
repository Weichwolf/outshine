"""A shared analytic first-hit measure; opaque homogeneous research control only."""

import json
import time
from pathlib import Path

import numpy as np
from adaptive_area_measure import compile_measure
from opaque_area_grid import AreaGrid
from visible_normal_bake import frame


def visible_partition(source, domain, up):
    eye = np.mean(domain, axis=0)
    basis = frame(source["positions"], domain, up)
    local = (source["positions"].astype(float) - eye) @ basis.T
    if local[:, 2].min() <= 0.2:
        raise ValueError("group requires another directional domain")
    xy = local[:, :2] / local[:, 2, None]
    extent = np.ptp(xy, axis=0)
    start, step = xy.min(0) - 0.01 * extent, 1.02 * extent
    grid = AreaGrid(source, eye, basis, start, step, [1, 1])
    pieces = list(grid.fragments())
    fragments = pieces[0][1] if pieces else []
    geometry = {
        "eye": eye,
        "basis": basis,
        "start": start,
        "step": step,
        "plane": np.array(2 / (1 / local[:, 2].min() + 1 / local[:, 2].max())),
        "observer_box": np.asarray(domain, float),
    }
    return fragments, geometry, grid.work


def components(fragments, pixel_scale):
    """Exclude unresolvable polygon remnants before further GEOS operations."""
    polygons, faces, excluded = [], [], []
    for face, original in fragments:
        stack = [original]
        while stack:
            part = stack.pop()
            if part.geom_type in ("GeometryCollection", "MultiPolygon"):
                stack.extend(part.geoms)
            elif part.geom_type == "Polygon":
                area = part.area * pixel_scale
                if area > 1e-12:
                    polygons.append(part)
                    faces.append(face)
                elif area > 0:
                    excluded.append((int(face), area))
    return (
        polygons,
        faces,
        {
            "conditioning_excluded_components": len(excluded),
            "conditioning_excluded_reported_area_px": sum(a for _, a in excluded),
            "conditioning_affected_faces": sorted({f for f, _ in excluded}),
            "conditioning_scope": "1e-12 projector-pixel component-area guard; reported mass is not a formal geometry-error bound. No native/general GEOS admission",
        },
    )


def cook(source, domain, up, profile, records, path, provenance, max_array_bytes):
    path = Path(path)
    path.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    fragments, geometry, work = visible_partition(source, domain, up)
    partition_s = time.perf_counter() - start
    pixel_scale = float(np.prod(geometry["step"]) * profile["focal_px"] ** 2)
    polygons, faces, conditioning = components(fragments, pixel_scale)
    records = sorted(records, key=lambda r: r["index"])
    sources = np.array(
        [(r["index"], r["kind"], r["id"]) for r in records],
        dtype=[("ordinal", "<u4"), ("kind", "u1"), ("id", "<u8")],
    )
    fixed = sources.nbytes + sum(a.nbytes for a in geometry.values())
    start = time.perf_counter()
    arrays, adaptive = compile_measure(
        polygons,
        source["normal"][source["indices"][faces, 0]],
        max_array_bytes=max_array_bytes,
        fixed_bytes=fixed,
    )
    adaptive_s = time.perf_counter() - start
    arrays.update(geometry, sources=sources)
    np.savez_compressed(path / "measure.npz", **arrays)
    header = {
        "format": "adaptive-opaque-normal-measure-1",
        "provenance": provenance,
        "observer_bounds": np.asarray(domain).tolist(),
        "profile": profile,
        "array_bytes": {k: a.nbytes for k, a in arrays.items()},
        "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
        "container_bytes": (path / "measure.npz").stat().st_size,
        "comparison_storage_limit_bytes": max_array_bytes,
        "partition_s": partition_s,
        "adaptive_s": adaptive_s,
        "partition_work": work,
        "adaptive_work": adaptive,
        "conditioning": conditioning,
        "l1_normal_measure_residual_projector_px": adaptive[
            "remaining_l1_measure_error"
        ]
        * pixel_scale,
        "scope": "one unlit anchored projected measure, not corner-view interpolation. Construction still uses supplied native originals; homogeneous opaque normal/coverage only, no native material/source-hit/depth/shadow/transition or global residency admission",
    }
    (path / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return arrays, header
