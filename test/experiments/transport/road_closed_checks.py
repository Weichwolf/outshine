"""Independent observations of native analytic normal offsets; sampled errors only."""

import numpy as np
from road_surface_frames import differentials, unit
from road_surface_model import Curve


def chunk_error(curve, body, faces, params):
    bary = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1], [0.2, 0.3, 0.5], [1 / 3] * 3])
    coordinates = np.einsum("sj,fji->fsi", bary, params[faces]).reshape(-1, 3)
    points = np.einsum(
        "sj,fji->fsi", bary, body["positions_enh_m"][faces].astype(float)
    ).reshape(-1, 3)
    s, u, depth = coordinates.T
    # Endpoint barycentric roundoff is not a request beyond the closed domain.
    s = np.clip(s, curve.stations[0], curve.stations[-1])
    u = np.clip(u, -curve.width, curve.width)
    upper = curve.at(s)
    center = (upper[:, 0] + upper[:, -1]) / 2
    direction = (upper[:, -1] - upper[:, 0]) / (2 * curve.width)
    values, inverse = np.unique(u, return_inverse=True)
    along, across = differentials(curve, s, lateral_m=values)
    normal = unit(np.cross(along, across))[np.arange(len(s)), inverse]
    truth = center + u[:, None] * direction - depth[:, None] * curve.thickness * normal
    return float(np.linalg.norm(points - truth, axis=1).max())


def observed_body_error(source, body, *, batch_faces=128):
    """Observe five points on every stored face with bounded live query arrays."""
    if type(batch_faces) is not int or not 1 <= batch_faces <= 8192:
        raise ValueError("Bounded closed body observation batch required")
    curve = Curve(source)
    ids = body["upper_source_vertex_ids"]
    stations = body["stations_m"][ids // 4]
    lateral = curve.across[ids % 4]
    params = np.c_[
        np.tile(stations, 2), np.tile(lateral, 2), np.repeat([0, 1], len(ids))
    ]
    faces = body["indices"]
    return max(
        chunk_error(curve, body, faces[start : start + batch_faces], params)
        for start in range(0, len(faces), batch_faces)
    )
