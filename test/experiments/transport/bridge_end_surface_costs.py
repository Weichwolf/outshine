"""Whole end-stage allocations and equal-footprint triangulation control."""

import time

import numpy as np
import shapely
from bridge_end_surface_geometry import strip_faces
from outshine_ref.transport.port_heights._world import normal_gradients


def traced(path, invoke):
    import memray

    with memray.Tracker(str(path), trace_python_allocators=True):
        invoke()
    reader = memray.FileReader(str(path))
    releases = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.MUNMAP,
        memray.AllocatorType.PYMALLOC_FREE,
    }
    requests = bytes_ = 0
    for row in reader.get_allocation_records():
        if row.allocator not in releases:
            requests += row.n_allocations
            bytes_ += row.size
    return {
        "allocation_requests": requests,
        "cumulative_requested_bytes": bytes_,
        "tracked_peak_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
    }


def cdt_faces(vertices, polygon):
    lookup = {tuple(p): i for i, p in enumerate(vertices[:, :2])}
    result = shapely.constrained_delaunay_triangles(polygon)
    coordinates = shapely.get_coordinates(result).reshape(-1, 4, 2)[:, :3]
    faces = np.array([[lookup[tuple(p)] for p in triangle] for triangle in coordinates])
    xy = vertices[faces, :2]
    a, b = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
    reverse = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] < 0
    faces[reverse] = faces[reverse][:, [0, 2, 1]]
    return faces


def triangulation_comparison(mesh, *, trace_root=None):
    rows = mesh.vertices.reshape(-1, 2, 3)
    polygon = shapely.Polygon(np.vstack((rows[:, 0, :2], rows[::-1, 1, :2])))
    methods = {
        "strip": lambda: strip_faces(len(rows)),
        "geos_cdt": lambda: cdt_faces(mesh.vertices, polygon),
    }
    results = {}
    reference = normal_gradients(
        mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
    )[0]
    for name, invoke in methods.items():
        # Warm and validate the two representations before separate stage timing.
        faces = invoke()
        tiles = shapely.polygons(mesh.vertices[faces, :2])
        assert shapely.coverage_is_valid(tiles)
        assert (
            shapely.symmetric_difference(
                shapely.coverage_union_all(tiles), polygon
            ).area
            <= 1e-9
        )
        cap_edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
        assert (cap_edges == [0, 1]).all(axis=1).sum() == 1
        grad = normal_gradients(mesh.vertices[:, :2], faces, mesh.vertices[:, 2])
        assert np.linalg.norm(grad - reference, axis=1).max() < 1e-8
        began = time.perf_counter()
        measured = invoke()
        result = {
            "elapsed_ms": 1000 * (time.perf_counter() - began),
            "faces": len(measured),
            "output_bytes": measured.nbytes,
            "geometry_kernel_triangulations": int(name == "geos_cdt"),
        }
        if trace_root is not None:
            result.update(
                traced(
                    trace_root.with_name(trace_root.name + "-" + name + ".bin"), invoke
                )
            )
        results[name] = result
    return {
        "methods": results,
        "scope": "Same prepared actual source-frame ribbon vertices and polygon, shared original cap, full footprint, whole plane gradients; independent validation outside timings. CDT mapping/orientation is included, polygon preparation excluded from both. Separate triangulation stage, not whole-builder or native/device speedup.",
    }
