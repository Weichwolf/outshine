"""Bounded XY distance for proposal ordering, never a clearance certificate."""

import numpy as np
import shapely
from stair_occupancy_geometry import OccupancyError


def lower_bounds(bounds, query):
    # L-infinity is a lower bound on Euclidean separation. Round differences down.
    delta = np.maximum(bounds[:, :2] - query[2:], query[:2] - bounds[:, 3:5])
    return np.maximum(0, np.nextafter(delta.max(1), -np.inf))


def nearest(catalog, geometry, budget, *, method="coarse"):
    if geometry.is_empty or not geometry.is_valid:
        raise OccupancyError("distance requires nonempty valid source geometry")
    query = np.array(geometry.bounds)
    if query.shape != (4,) or not np.isfinite(query).all():
        raise OccupancyError("distance requires finite XY bounds")
    work = {
        "products": 0,
        "face_tests": 0,
        "geometry_candidates": 0,
        "volume_materializer_calls": 0,
        "prune_slack_m": 1e-9,
    }
    if len(catalog.sources) > budget.products:
        raise OccupancyError("distance product budget exhausted")
    if not catalog.sources:
        raise OccupancyError("distance catalog has no verified facets")
    if method not in ("flat", "coarse"):
        raise ValueError("bounded numeric distance method required")
    if method == "flat":
        groups = [(0.0, catalog.sources)]
    else:
        bounds = lower_bounds(np.asarray(catalog.envelopes), query)
        bounds[[i for i, source in enumerate(catalog.sources) if source.always]] = 0
        groups = [
            (bounds[i], (catalog.sources[i],))
            for i in np.argsort(bounds, kind="stable")
        ]
    best = np.inf
    try:
        for bound, sources in groups:
            if bound > np.nextafter(best + 1e-9, np.inf):
                break
            distances, starts = [], [0]
            for source in sources:
                if work["face_tests"] + len(source.bounds) > budget.face_tests:
                    raise OccupancyError("distance metadata face budget exhausted")
                work["products"] += 1
                work["face_tests"] += len(source.bounds)
                distances.append(
                    np.zeros(len(source.bounds))
                    if source.always
                    else lower_bounds(source.bounds, query)
                )
                starts.append(starts[-1] + len(source.bounds))
            distances = np.concatenate(distances)
            for index in np.argsort(distances, kind="stable"):
                if distances[index] > np.nextafter(best + 1e-9, np.inf):
                    break
                if work["geometry_candidates"] >= budget.candidates:
                    raise OccupancyError("distance candidate budget exhausted")
                work["geometry_candidates"] += 1
                position = int(np.searchsorted(starts, index, side="right") - 1)
                source = sources[position]
                face = int(index) - starts[position]
                best = min(best, selected_distance(source, face, geometry, work))
                if best == 0:
                    break
            if best == 0:
                break
    except (ValueError, shapely.errors.GEOSException) as error:
        raise OccupancyError("bounded XY distance unresolved: " + str(error)) from error
    if not np.isfinite(best):
        raise OccupancyError("no verified XY distance found")
    return best, work


def selected_distance(source, face, geometry, work):
    if source.xy_geometry is None:
        work["volume_materializer_calls"] += 1
        shapes = [row.geometry for row, _keys, _error in source.materialize(face)]
    else:
        shapes = [source.xy_geometry(face)]
    best = np.inf
    for shape in shapes:
        if shape.is_empty or not shape.is_valid:
            raise OccupancyError("invalid selected XY footprint")
        if not source.always:
            bounds = source.bounds[face]
            xy = np.array(shape.bounds)
            if np.any(xy[:2] < bounds[:2]) or np.any(xy[2:] > bounds[3:5]):
                raise OccupancyError("XY footprint escapes numeric metadata")
        distance = float(geometry.distance(shape))
        if not np.isfinite(distance) or distance < 0:
            raise OccupancyError("distance geometry unresolved")
        best = min(best, distance)
    return best
