"""One-view unlit depth samples; no opaque-coverage or PBR certificate.

The minimal sprite-with-depth proposal stores one front sample per capture ray.
Geometric normal controls support; native shader normal/material inputs stay
separate. Provenance binds sample centres, never their extended footprints.
"""

import numpy as np
import trimesh
from packed_normals import fold, unpack
from source_billboard import image_rays, projected_bounds
from source_far_reprojection import intersect_source
from source_surface_fields import surface_fields

RECORD = np.dtype(
    [
        ("pixel", "<u4"),
        ("depth", "<f4"),
        ("geometric_normal", "<u4"),
        ("normal", "<u4"),
        ("code", "<i2"),
        ("domain", "u1"),
        ("span", "u1"),
        ("uv", "<f4", 2),
        ("metres", "<f4", 2),
        ("colour", "u1", 4),
    ]
)
TRACE = np.dtype([("face", "<u4"), ("weights", "<u4", 3)])


def reference(source, prepared, origins, directions):
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    fields = surface_fields(source, faces)
    bary = np.zeros((len(faces), 3))
    valid = faces >= 0
    triangles = source["positions"][source["indices"][faces[valid]]].astype(float)
    points = origins[valid] + depth[valid, None] * directions[valid]
    bary[valid] = trimesh.triangles.points_to_barycentric(triangles, points)
    for key in ("uv", "metres"):
        fields[key] = np.zeros((len(faces), 2))
        if source[key] is not None:
            fields[key][valid] = np.einsum(
                "ni,nij->nj", bary[valid], source[key][source["indices"][faces[valid]]]
            )
    fields["domain"] = np.full(len(faces), 255, np.uint8)
    fields["domain"][valid] = source["domain"][faces[valid]]
    return depth, faces, fields, bary


def encode_normal(normal):
    xy = np.floor((fold(normal) * 0.5 + 0.5) * 65535 + 0.5).astype(np.uint32)
    return (xy[:, 0] << 16) | xy[:, 1]


def bake(source, prepared, view, *, adaptive=False):
    box = projected_bounds(source["positions"], view)
    lo = np.maximum(np.floor(box[0]).astype(int), 0)
    hi = np.minimum(np.ceil(box[1]).astype(int), view[3:])
    width, height = hi - lo
    if width <= 0 or height <= 0:
        raise ValueError("empty capture")
    origins, directions = image_rays(view, np.array([lo, hi]), width, height, 2)
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    spans = np.isfinite(depth).astype(np.uint8)
    merged = 0
    if adaptive:
        y, x = np.indices((height, width))
        base = (y * 2 * width * 2 + x * 2).ravel()
        block = base[:, None] + [0, 1, width * 2, width * 2 + 1]
        same_face = (faces[block[:, 0]] >= 0) & (
            faces[block] == faces[block[:, :1]]
        ).all(1)
        spans[block[same_face].ravel()] = 0
        spans[base[same_face]] = 2
        merged = int(same_face.sum())
    pixels = np.flatnonzero(spans)
    if not len(pixels):
        raise ValueError("empty depth cache cannot demonstrate quality")
    face = faces[pixels]
    triangles = source["positions"][source["indices"][face]].astype(float)
    geometric = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    geometric /= np.linalg.norm(geometric, axis=1, keepdims=True)
    ray = directions[pixels].copy()
    selected_depth = depth[pixels].copy()
    coarse = spans[pixels] == 2
    # Four front hits on one original triangle are replaced by its exact
    # centre-plane intersection, never by blending depths from different faces.
    xy = np.column_stack((pixels[coarse] % (width * 2), pixels[coarse] // (width * 2)))
    centre = lo + (xy + 1) / 2
    eye, basis, focal, screen_width, screen_height = view
    ray[coarse] = (
        np.column_stack(
            (
                (centre[:, 0] - screen_width / 2) / focal,
                -(centre[:, 1] - screen_height / 2) / focal,
                np.ones(len(centre)),
            )
        )
        @ basis
    )
    numerator = np.einsum("ni,ni->n", geometric[coarse], triangles[coarse, 0] - eye)
    selected_depth[coarse] = numerator / np.einsum(
        "ni,ni->n", geometric[coarse], ray[coarse]
    )
    point = eye + selected_depth[:, None] * ray
    bary = trimesh.triangles.points_to_barycentric(triangles, point)
    # Visibility and optional density selection precede material field gathers.
    data = np.zeros(len(pixels), RECORD)
    data["pixel"], data["depth"], data["span"] = pixels, selected_depth, spans[pixels]
    data["normal"], data["geometric_normal"] = (
        source["normal"][face],
        encode_normal(geometric),
    )
    data["code"], data["domain"] = source["code"][face], source["domain"][face]
    for key in ("uv", "metres"):
        if source[key] is not None:
            data[key] = np.einsum(
                "ni,nij->nj", bary, source[key][source["indices"][face]]
            )
    colour = source["palette"][source["colour_index"][face]]
    data["colour"] = np.floor(np.clip(colour, 0, 1) * 255 + 0.5).astype(np.uint8)
    weights = np.clip(np.floor(bary[:, 1:] * (1 << 30)), 0, 1 << 30).astype(np.uint32)
    weights[:, 1] = np.minimum(weights[:, 1], (1 << 30) - weights[:, 0])
    trace = np.zeros(len(data), TRACE)
    trace["face"] = face
    trace["weights"] = np.column_stack(((1 << 30) - weights.sum(1), weights))
    saved = origins[pixels] + data["depth"][:, None] * ray
    held = np.einsum(
        "ni,nij->nj", trace["weights"].astype(float) / (1 << 30), triangles
    )
    return {
        "data": data,
        "trace": trace,
        "view": view,
        "lo": lo,
        "width": int(width),
        "height": int(height),
        "supersample": 2,
        "source_rays": len(origins),
        "front_capture_samples": int(np.isfinite(depth).sum()),
        "material_records_gathered": len(data),
        "merged_full_pixels": merged,
        "density": "same-original-triangle merge" if adaptive else "uniform half-pixel",
        "has_metres": source["metres"] is not None,
        "membership": source["metadata"],
        "sample_binding_max_residual_m": float(
            np.linalg.norm(saved - held, axis=1).max()
        ),
        "depth_rounding_max_m": float(
            abs(data["depth"].astype(float) - selected_depth).max()
        ),
    }


def fields(cache, slots):
    valid = slots >= 0
    data, selected = cache["data"], slots[valid]
    result = {
        key: np.zeros((len(slots), n))
        for key, n in (("normal", 3), ("colour", 3), ("uv", 2), ("metres", 2))
    }
    result["normal"][valid] = unpack(data["normal"][selected])
    result["colour"][valid] = data["colour"][selected, :3].astype(float) / 255
    for key in ("uv", "metres"):
        result[key][valid] = data[key][selected]
    result["code"] = np.zeros(len(slots), np.int16)
    result["code"][valid] = data["code"][selected]
    result["domain"] = np.full(len(slots), 255, np.uint8)
    result["domain"][valid] = data["domain"][selected]
    return result


def prepare(cache):
    data, view = cache["data"], cache["view"]
    eye, basis, focal, screen_width, screen_height = view
    width = cache["width"] * cache["supersample"]
    xy = np.column_stack((data["pixel"] % width, data["pixel"] // width)).astype(float)
    span = data["span"].astype(float)
    centre = cache["lo"] + (xy + span[:, None] / 2) / cache["supersample"]
    corners = (
        cache["lo"]
        + (
            xy[:, None, :]
            + span[:, None, None] * np.array([[0, 0], [1, 0], [1, 1], [0, 1]])
        )
        / cache["supersample"]
    )

    def rays(p):
        return (
            np.stack(
                (
                    (p[..., 0] - screen_width / 2) / focal,
                    -(p[..., 1] - screen_height / 2) / focal,
                    np.ones(p.shape[:-1]),
                ),
                axis=-1,
            )
            @ basis
        )

    normal = unpack(data["geometric_normal"])
    point = eye + data["depth"][:, None] * rays(centre)
    direction = rays(corners)
    denominator = np.einsum("ni,nki->nk", normal, direction)
    numerator = np.einsum("ni,ni->n", normal, point - eye)
    t = np.divide(
        numerator[:, None],
        denominator,
        out=np.zeros_like(denominator),
        where=abs(denominator) > 1e-12,
    )
    valid = (denominator < -1e-12).all(1) & (t > 0.1).all(1)
    result = {
        "normal": normal,
        "point": point,
        "corners": eye + t[:, :, None] * direction,
        "valid": valid,
    }
    result["array_bytes"] = sum(v.nbytes for v in result.values())
    return result
