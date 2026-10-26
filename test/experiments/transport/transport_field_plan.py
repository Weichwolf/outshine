"""Separate bounded height degrees of freedom from dense transport boundary geometry."""

import numpy as np
import shapely
from bounded_ground_fit import fit
from ground_affine_bounds import joined, overlap_bounds
from ground_surface import ground_layout
from ground_surface_showcase import clipped
from outshine_ref.contracts import Mesh


def bounds_for(xy, faces, road_faces, upper, roof, retained, *, coarse=False):
    maximum = 8192 if coarse else 262144
    records, reports = [], []
    for selected, barrier, offset, sign in (
        (road_faces, upper, -4.85, 1),
        (faces, upper, -0.37, 1),
        (faces, roof, 2.25, -1),
        (road_faces, retained, 0, 1),
    ):
        bound, report = overlap_bounds(
            xy,
            selected,
            barrier,
            offset_m=offset,
            sign=sign,
            seconds=8,
            max_faces=maximum,
        )
        records.append(bound)
        reports.append(report)
    return joined(*records), reports


def dense_input(traffic, domain, upper, roof, retained, prior):
    layout, partition = ground_layout(
        traffic.vertices[:, :2], traffic.triangles, domain, max_faces=262144
    )
    xy, faces, count = layout["xy"], layout["faces"], layout["road_faces"]
    frames = np.broadcast_to(np.eye(2) / (2 / 3), (len(faces), 2, 2)).copy()
    frames[:count] = np.eye(2) / 0.025
    bounds, reports = bounds_for(xy, faces, faces[:count], upper, roof, retained)
    targets, receipts = prior(xy)
    arrays = {
        "xy": xy,
        "faces": faces,
        "frames": frames,
        "targets": targets,
        "bound_columns": bounds[0],
        "bound_coefficients": bounds[1],
        "bound_limits": bounds[2],
    }
    return arrays, {"partition": partition, "bounds": reports, "dem_receipts": receipts}


def field_input(
    seed, traffic_shapes, traffic_domain, domain, upper, roof, retained, prior
):
    support = shapely.box(*domain.bounds)
    layout, partition = ground_layout(
        seed.vertices[:, :2], seed.triangles, support, max_faces=8192
    )
    xy, faces = layout["xy"], layout["faces"]
    pairs = shapely.STRtree(traffic_shapes).query(
        shapely.polygons(xy[faces]), predicate="intersects"
    )
    if pairs.shape[1] > 1048576:
        raise ValueError("field road-mask pair budget")
    selected = np.zeros(len(faces), bool)
    selected[np.unique(pairs[0])] = True
    frames = np.broadcast_to(np.eye(2) / (2 / 3), (len(faces), 2, 2)).copy()
    # Keep a construction margin, then check every actually emitted fine face.
    frames[selected] = np.eye(2) / 0.0249
    road_upper, _slots, _error = clipped(
        upper, np.zeros(len(upper.triangles)), traffic_domain
    )
    bounds = []
    reports = []
    for barrier, offset, sign in (
        (road_upper, -4.85, 1),
        (upper, -0.37, 1),
        (roof, 2.25, -1),
        (retained, 0, 1),
    ):
        record, report = overlap_bounds(
            xy, faces, barrier, offset_m=offset, sign=sign, seconds=8
        )
        bounds.append(record)
        reports.append(report)
    bound = joined(*bounds)
    targets, receipts = prior(xy)
    arrays = {
        "xy": xy,
        "faces": faces,
        "frames": frames,
        "targets": targets,
        "bound_columns": bound[0],
        "bound_coefficients": bound[1],
        "bound_limits": bound[2],
    }
    return arrays, {
        "partition": partition,
        "mask_pairs": pairs.shape[1],
        "road_cones": int(selected.sum()),
        "bounds": reports,
        "dem_receipts": receipts,
        "support_scope": "Planning rectangle only; actual world/portal domains are clipped separately. No extra source tile or rendered land.",
    }


def solve_input(arrays):
    heights, report = fit(
        arrays["xy"],
        arrays["faces"],
        arrays["targets"],
        arrays["frames"],
        tuple(
            arrays[k] for k in ("bound_columns", "bound_coefficients", "bound_limits")
        ),
        np.ones(len(arrays["xy"])),
        seconds=8,
        max_elements=262144,
    )
    return (
        None if heights is None else Mesh(np.c_[arrays["xy"], heights], arrays["faces"])
    ), report
