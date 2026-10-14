"""Bounded source-polyline strips with explicit stations and complete-face audits."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from surface_height_checks import normal_gradients


def source_frame(xy, width):
    if len(xy) < 2 or len(xy) > 256 or not np.isfinite(width) or not 0 < width <= 30:
        raise ValueError("bounded source axis and width required")
    xy = np.asarray(xy, float)
    if xy.shape != (len(xy), 2) or not np.isfinite(xy).all():
        raise ValueError("finite XY axis required")
    edges = np.diff(xy, axis=0)
    lengths = np.linalg.norm(edges, axis=1)
    if np.any(lengths < 1e-6):
        raise ValueError("repeated source station")
    tangent = edges / lengths[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    if len(tangent) > 1 and np.any(
        np.sum(tangent[:-1] * tangent[1:], axis=1) < np.cos(np.pi / 6)
    ):
        raise ValueError("source turn exceeds local strip recipe")
    miter = normal.copy()
    if len(tangent) > 1:
        middle = (normal[:-1] + normal[1:]) / (
            1 + np.sum(tangent[:-1] * tangent[1:], axis=1)
        )[:, None]
        miter = np.vstack((normal[0], middle, normal[-1]))
    else:
        miter = np.vstack((normal[0], normal[-1]))
    station = np.r_[0.0, np.cumsum(lengths)]
    sides = (
        xy[:, None, :]
        + np.array([-width / 2, width / 2])[None, :, None] * miter[:, None, :]
    )
    footprint = shapely.Polygon(np.vstack((sides[:, 0], sides[::-1, 1])))
    if not footprint.is_valid or footprint.is_empty:
        raise ValueError("source strip folds or self-intersects")
    return {
        "xy": xy,
        "station": station,
        "tangent": tangent,
        "miter": miter,
        "footprint": footprint,
    }


def ribbon(frame, table, width, crossfall):
    if len(table) > 4096:
        raise ValueError("strip vertex budget exhausted")
    table = np.asarray(table, float)
    stations = table[:, 0]
    if (
        np.any(np.diff(stations) <= 0)
        or stations[0] != 0
        or abs(stations[-1] - frame["station"][-1]) > 1e-7
    ):
        raise ValueError("full increasing source stations required")
    centres = np.column_stack(
        [np.interp(stations, frame["station"], frame["xy"][:, i]) for i in range(2)]
    )
    miters = np.column_stack(
        [np.interp(stations, frame["station"], frame["miter"][:, i]) for i in range(2)]
    )
    offsets = np.array([-width / 2, width / 2])
    xy = centres[:, None, :] + offsets[None, :, None] * miters[:, None, :]
    z = table[:, 1, None] + offsets * crossfall
    vertices = np.c_[xy.reshape(-1, 2), z.reshape(-1)]
    first = 2 * np.arange(len(table) - 1)
    faces = np.vstack(
        (np.c_[first, first + 2, first + 1], np.c_[first + 2, first + 3, first + 1])
    )
    return Mesh(vertices, faces)


def audit_ribbon(frame, mesh):
    xy = mesh.vertices[:, :2]
    triangles = shapely.polygons(xy[mesh.triangles])
    if not shapely.coverage_is_valid(triangles):
        raise ValueError("strip has overlapping or nonconforming triangles")
    union = shapely.coverage_union_all(triangles)
    error = float(union.symmetric_difference(frame["footprint"]).area)
    if error > max(1e-7, frame["footprint"].area * 1e-10):
        raise ValueError("strip changed complete source footprint")
    gradients = normal_gradients(xy, mesh.triangles, mesh.vertices[:, 2])
    return gradients, error


def grade_audit(frame, mesh, table, grade):
    gradients, error = audit_ribbon(frame, mesh)
    midpoint = (table[:-1, 0] + table[1:, 0]) / 2
    segments = np.searchsorted(frame["station"], midpoint, side="right") - 1
    direction = np.vstack((frame["tangent"][segments], frame["tangent"][segments]))
    longitudinal = np.sum(gradients * direction, axis=1) / grade
    cross = np.sum(gradients * np.c_[-direction[:, 1], direction[:, 0]], axis=1) / 0.025
    maximum = float(np.hypot(longitudinal, cross).max())
    if maximum > 1.0000001:
        raise ValueError("full strip face exceeds elliptic grade budget")
    return {"maximum_normalized_grade": maximum, "coverage_error_m2": error}
