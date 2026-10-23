"""Existing BuildingMesh shading rules on source triangles; unknown welds stay tagged."""

import numpy as np
from prepared_material_fields import facade_catalog
from source_material_patches import corner_fields, round_away, weld_local


def pack_float32(normals):
    """StoredVertex/Octahedral scalar Float32 operation order, without FMA claims."""
    v = np.asarray(normals)
    if (
        v.dtype != np.float32
        or v.ndim != 2
        or v.shape[1] != 3
        or not np.isfinite(v).all()
    ):
        raise ValueError("finite Float32 normal rows required")
    magnitude = abs(v)
    total = (magnitude[:, 0] + magnitude[:, 1]) + magnitude[:, 2]
    if (total == 0).any():
        raise ValueError("nonzero normal rows required")
    by = np.float32(1) / total
    xy = v[:, :2] * by[:, None]
    folded = (np.float32(1) - abs(xy[:, ::-1])) * np.where(
        xy >= 0, np.float32(1), np.float32(-1)
    )
    xy = np.where(v[:, 2:3] < 0, folded, xy)
    bounded = np.clip(xy, np.float32(-1), np.float32(1))
    scaled = (bounded * np.float32(0.5) + np.float32(0.5)) * np.float32(65535)
    pair = round_away(scaled).astype(np.uint32)
    return (pair[:, 0] << 16) | pair[:, 1]


def wall_colours(uv, streams, colour, *, catalog=None):
    """AppendWallColours, including its exact UV predicate and Float32 ratio."""
    uv = np.asarray(uv, np.float32)
    streams = np.broadcast_to(np.asarray(streams), (len(uv),))
    result = np.ones((len(uv), 4), np.float32)
    if colour is None:
        return result
    c = facade_catalog() if catalog is None else catalog
    u = uv[:, 0]
    code = np.trunc(u / np.float32(256)).astype(np.int64)
    field = np.trunc(u / np.float32(256 * len(c))).astype(np.int64) % 3
    glazing = (code % len(c) == c["Glazing"]) | (field == 1)
    negative_wall = np.fmod(-u - np.float32(1), np.float32(16)) == 0
    painted = (streams == 0) & (((u >= 0) & ~glazing) | negative_wall)
    result[painted, :3] = np.asarray(colour, np.float32) / np.array(
        [0.74, 0.71, 0.65], np.float32
    )
    return result


def curved_normals(shape, points, geometric, *, catalog):
    """BuildingWallShadingNormal, evaluated at snapped corners, not new XYZ."""
    result = np.broadcast_to(geometric[:, None], points.shape).copy()
    closed = shape["opening_style"] in [
        catalog[k] for k in ("Tower", "Outbuilding", "Hall")
    ]
    curved = (
        shape["form"] == 5
        and closed
        and not shape["holes"]
        and shape["round_footprint"]
        and len(shape["ring"]) >= 8
    )
    if not curved:
        return result
    relative = points[..., :2] - np.asarray(shape["centre"])
    u = np.asarray(shape["axis_u"])
    v = np.array([-u[1], u[0]])
    along_u = relative[..., 0] * u[0] + relative[..., 1] * u[1]
    along_v = -relative[..., 0] * u[1] + relative[..., 1] * u[0]
    east = (
        along_u / shape["half_u_m"] ** 2 * u[0]
        + along_v / shape["half_v_m"] ** 2 * v[0]
    )
    north = (
        along_u / shape["half_u_m"] ** 2 * u[1]
        + along_v / shape["half_v_m"] ** 2 * v[1]
    )
    length = np.hypot(east, north)
    use = (abs(geometric[:, 2:3]) <= 1e-6) & (length > 0)
    result[..., 0] = np.where(
        use,
        np.divide(east, length, out=np.zeros_like(east), where=length > 0),
        result[..., 0],
    )
    result[..., 1] = np.where(
        use,
        np.divide(north, length, out=np.zeros_like(north), where=length > 0),
        result[..., 1],
    )
    result[..., 2] = np.where(use, 0, result[..., 2])
    return result


def shading_fields(patch, parameters, *, catalog=None, uv=None, include_colour=True):
    """Reproduce source-triangle rules, not the original native triangulation.

    Invalid normal words are zero sentinels guarded by face_shading_valid. No
    native weld pruning or missing normal is replaced by an invented average.
    """
    q = np.asarray(parameters, float)
    if q.ndim != 3 or q.shape[1:] != (3, 2) or not np.isfinite(q).all():
        raise ValueError("finite source triangles in patch parameters required")
    local = patch.local_origin + q @ patch.local_axes
    points = weld_local(local)
    before = np.cross(local[:, 1] - local[:, 0], local[:, 2] - local[:, 0])
    expected = patch.orientation * np.cross(*patch.local_axes)
    reverse = (before @ expected) < 0
    normal = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    normal[reverse] *= -1
    length = np.sqrt((normal[:, 0] ** 2 + normal[:, 1] ** 2) + normal[:, 2] ** 2)
    valid = length >= 1e-12
    normal = np.divide(
        normal, length[:, None], out=np.zeros_like(normal), where=valid[:, None]
    )
    c = facade_catalog() if catalog is None else catalog
    shading = curved_normals(patch.shape, points, normal, catalog=c)
    frame = np.array([patch.surface[k] for k in ("east", "north", "up")])
    world = (
        (shading[..., 0, None] * frame[0] + shading[..., 1, None] * frame[1])
        + shading[..., 2, None] * frame[2]
    ).astype(np.float32)
    words = np.zeros((len(q), 3, 1), np.uint32)
    words[valid, :, 0] = pack_float32(world[valid].reshape(-1, 3)).reshape(-1, 3)
    uv = corner_fields(patch, q.reshape(-1, 2), catalog=c)[0] if uv is None else uv
    first_u = uv.reshape(-1, 3, 2)[:, 0, 0]
    trim = (first_u < 0) & (np.fmod(-first_u - np.float32(1), np.float32(16)) == 5)
    streams = np.where(
        valid, ((normal[:, 2] > 0.5) & ~trim).astype(np.int8), -1
    ).astype(np.int8)
    result = {
        "corner_shading_words": words,
        "face_shading_valid": valid,
        "face_native_stream": streams,
    }
    if include_colour:
        result["corner_colour"] = wall_colours(
            uv, np.repeat(streams, 3), patch.surface["wall_colour"], catalog=c
        ).reshape(-1, 3, 4)
    return result
