"""Independent unfitted perimeter probes; sampled errors are not global bounds."""

import numpy as np
import shapely


def ordered_rim(result):
    if "rim_profile" in result:
        return result["mesh"].vertices[result["rim_profile"]["ordered_vertex_ids"]]
    xy = np.asarray(result["domain"].exterior.coords)[:-1]
    vertices = result["mesh"].vertices[result["outer_vertex_ids"]]
    lookup = {tuple(p[:2]): p for p in vertices}
    return np.array([lookup[tuple(p)] for p in xy])


def validation_points(result, count=512):
    xy = np.asarray(shapely.box(*result["domain"].bounds).exterior.coords)[:-1]
    arc = np.r_[0.0, np.cumsum(np.linalg.norm(np.roll(xy, -1, axis=0) - xy, axis=1))]
    at = (np.arange(count) + 0.37) / count * arc[-1]
    queries = np.c_[
        np.interp(at, arc, np.r_[xy[:, 0], xy[0, 0]]),
        np.interp(at, arc, np.r_[xy[:, 1], xy[0, 1]]),
    ]
    return at, queries


def validate_rim(result, at, observed):
    vertices = ordered_rim(result)
    arc = np.r_[
        0.0,
        np.cumsum(
            np.linalg.norm(
                np.roll(vertices[:, :2], -1, axis=0) - vertices[:, :2], axis=1
            )
        ),
    ]
    predicted = np.interp(at, arc, np.r_[vertices[:, 2], vertices[0, 2]])
    delta = predicted - observed
    return {
        "count": len(at),
        "maximum_abs_error_m": float(abs(delta).max()),
        "rms_error_m": float(np.sqrt(np.mean(delta * delta))),
        "scope": "Same unfitted geometric perimeter queries against the bound DEM; sampled error only, no continuous/global/original-grid error certificate.",
    }, predicted
