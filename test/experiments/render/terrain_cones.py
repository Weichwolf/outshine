"""Measure meshoptimizer normal-cone rejection of unchanged cached terrain patches."""

import argparse
import hashlib
import csv
import ctypes as C
import ctypes.util
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds, project, raster
from terrain_grid_lod import page_frame, page_geometry, raster_view, triangles
from terrain_visibility import partition


class Bounds(C.Structure):
    _fields_ = [
        ("center", C.c_float * 3),
        ("radius", C.c_float),
        ("apex", C.c_float * 3),
        ("axis", C.c_float * 3),
        ("cutoff", C.c_float),
        ("axis_s8", C.c_byte * 3),
        ("cutoff_s8", C.c_byte),
    ]


def cluster_bounds(library=None):
    path = (
        str(library)
        if library is not None
        else ctypes.util.find_library("meshoptimizer")
    )
    if not path:
        raise RuntimeError("meshoptimizer shared library required (Homebrew/apt)")
    library = C.CDLL(path)
    function = library.meshopt_computeClusterBounds
    function.argtypes = [
        C.POINTER(C.c_uint),
        C.c_size_t,
        C.POINTER(C.c_float),
        C.c_size_t,
        C.c_size_t,
    ]
    function.restype = Bounds
    if C.sizeof(Bounds) != 48 or Bounds.axis_s8.offset != 44:
        raise RuntimeError("meshoptimizer C ABI layout mismatch")
    return function


def reject(function, points, faces, eye, padding):
    vertices = np.ascontiguousarray(points, dtype=np.float32)
    indices = np.ascontiguousarray(faces, dtype=np.uint32)
    bounds = function(
        indices.ctypes.data_as(C.POINTER(C.c_uint)),
        indices.size,
        vertices.ctypes.data_as(C.POINTER(C.c_float)),
        len(vertices),
        vertices.strides[0],
    )
    direction = np.array(bounds.apex) - eye
    length = np.linalg.norm(direction)
    if (
        not np.isfinite(direction).all()
        or not np.isfinite(bounds.cutoff)
        or length <= 1e-8
    ):
        return False
    return bool(direction @ np.array(bounds.axis) >= (bounds.cutoff + padding) * length)


def compare(pages, scenario, width, height, yaw, cells, padding, library=None):
    if (
        cells not in (4, 8)
        or min(width, height) <= 0
        or not np.isfinite(padding)
        or padding < 0
    ):
        raise ValueError(
            "positive image dimensions, 4/8-cell patches and nonnegative finite padding required"
        )
    started = time.perf_counter()
    function = cluster_bounds(library)
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    view = raster_view(scenario, rows, pages, width, height, yaw)
    patches, nodes = partition(cells)
    local_faces = triangles(cells + 1, 1)
    baseline = triangles(33, 1)
    full = np.zeros((height, width))
    retained = full.copy()
    candidate = rejected = oracle_rejected = unsafe = 0
    for row in rows:
        _, local, _, world, stamp = page_geometry(row, pages)
        fingerprint.update(stamp)
        centre, axes, _ = page_frame(row)
        eye = (view[0] - centre) @ axes.T
        points = world[nodes]
        _, _, _, _, visible, _ = box_bounds(
            (points.min(axis=1), points.max(axis=1)), view
        )
        keep = visible.copy()
        candidate += int(visible.sum())
        for patch in np.flatnonzero(visible):
            source = local[nodes[patch]]
            p = source[local_faces]
            normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
            all_back = np.all(np.einsum("ni,ni->n", normals, eye - p[:, 0]) <= 0)
            oracle_rejected += int(all_back)
            if reject(function, source, local_faces, eye, padding):
                rejected += 1
                unsafe += int(not all_back)
                keep[patch] = False
        projected, z = project(world, view)
        full = np.maximum(full, raster(projected, z, baseline, width, height))
        retained = np.maximum(
            retained, raster(projected, z, patches[keep].reshape(-1, 3), width, height)
        )
    difference = np.abs(full - retained)
    return (
        dict(
            pages=len(rows),
            input_sha256=fingerprint.hexdigest(),
            scenario_sha256=hashlib.sha256(scenario.read_bytes()).hexdigest(),
            width=width,
            height=height,
            yaw=yaw,
            cells=cells,
            padding=padding,
            candidate_patches=candidate,
            rejected_patches=rejected,
            exact_backfacing_patches=oracle_rejected,
            unsafe_rejected_patches=unsafe,
            candidate_triangles=candidate * cells * cells * 2,
            rejected_triangles=rejected * cells * cells * 2,
            changed_pixels=int(np.count_nonzero(difference > 1e-12)),
            max_depth_difference=float(difference.max()),
            seconds=time.perf_counter() - started,
            scope="Installed meshoptimizer C API on float32 local points; double-source triangle oracle and opaque interior raster. Shader rounding, native culler cost and skirts remain separate.",
        ),
        full,
        retained,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--cells", type=int, choices=[4, 8], default=8)
    parser.add_argument("--padding", type=float, default=1e-4)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    args.output.mkdir(parents=True, exist_ok=True)
    report, full, retained = compare(
        args.pages,
        args.scenario,
        args.width,
        args.height,
        args.yaw,
        args.cells,
        args.padding,
        args.library,
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(args.output / "depths.npz", reference=full, retained=retained)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
