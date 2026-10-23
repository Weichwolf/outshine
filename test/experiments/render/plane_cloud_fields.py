"""Sampled unlit chart fields and transparent CPU queries, not native PBR."""

import numpy as np
from current_visibility import raster
from field_sampling import bilinear
from native_capture_meshes import sample


def bake(source, chart):
    """Rasterize every assigned triangle orthographically, including hidden faces.

    Within one slab only the oriented nearest projected face supplies a texel.
    The resulting lost ordering and material mixtures remain measured errors.
    """
    xyz = source["positions"].astype(float)
    points = xyz @ chart.basis.T
    h, w = chart.shape
    projected = np.column_stack(
        (
            2 * (points[:, 0] - chart.low[0]) / chart.pitch,
            2 * h - 2 * (points[:, 1] - chart.low[1]) / chart.pitch,
            points[:, 2] - points[:, 2].min() + 1,
        )
    )
    work = {}
    _z, owners = raster(
        projected,
        np.ones(len(xyz)),
        source["indices"][chart.faces],
        2 * w,
        2 * h,
        with_faces=True,
        counters=work,
    )
    valid = owners.ravel() >= 0
    locations = np.flatnonzero(valid)
    y, x = np.divmod(locations, 2 * w)
    uv = chart.low + np.column_stack(((x + 0.5) / 2, h - (y + 0.5) / 2)) * chart.pitch
    triangles = source["indices"][chart.faces[owners.ravel()[valid]]]
    q = points[triangles, :2]
    a, b, c = q.transpose(1, 0, 2)
    e, f, r = b - a, c - a, uv - a
    cross = lambda p, q: p[:, 0] * q[:, 1] - p[:, 1] * q[:, 0]
    den = cross(e, f)
    s, t = cross(r, f) / den, cross(e, r) / den
    weights = np.column_stack((1 - s - t, s, t))
    normal = np.einsum("ij,ijk->ik", weights, source["vertex_normal"][triangles])
    norm = np.linalg.norm(normal, axis=1)
    if (norm == 0).any():
        raise ValueError("undefined source normal in chart")
    normal /= norm[:, None]
    colour = np.einsum("ij,ijk->ik", weights, source["colour"][triangles, :3])
    fields = np.zeros((4 * h * w, 7))
    fields[valid, :3], fields[valid, 3], fields[valid, 4:] = colour, 1, normal
    mean = fields.reshape(h, 2, w, 2, 7).mean((1, 3))
    alpha = mean[:, :, 3:4]
    colour = np.divide(mean[:, :, :3], alpha, out=np.zeros((h, w, 3)), where=alpha > 0)
    normal = np.divide(mean[:, :, 4:], alpha, out=np.zeros((h, w, 3)), where=alpha > 0)
    rgba = np.floor(
        np.clip(np.concatenate((colour, alpha), axis=2), 0, 1) * 255 + 0.5
    ).astype(np.uint8)
    moment = np.dstack((normal, np.zeros((h, w)))).astype(np.float16)
    decoded = rgba.astype(float) / 255
    payload = np.concatenate(
        (
            decoded[:, :, :3] * decoded[:, :, 3:4],
            decoded[:, :, 3:4],
            moment[:, :, :3].astype(float) * decoded[:, :, 3:4],
        ),
        axis=2,
    )
    work.update(
        assigned_faces=len(chart.faces),
        source_field_samples=len(locations),
        nonzero_alpha_texels=int((rgba[:, :, 3] > 0).sum()),
        base_texture_bytes=rgba.nbytes + moment.nbytes,
        decoded_cpu_payload_bytes=payload.nbytes,
    )
    return {
        "chart": chart,
        "rgba": rgba,
        "moment": moment,
        "payload": payload,
        "work": work,
    }


def query(baked, fallback, prepared, origins, directions, suns):
    """Evaluate stored rectangles/fields and original fallback at identical rays."""
    if len(origins) > 512 or len(baked) > 256:
        raise ValueError("query requires bounded ray/chart batches")
    count, layers = len(origins), len(baked) + 1
    depths = np.full((count, layers), np.inf)
    alpha = np.zeros((count, layers))
    colours = np.zeros((count, layers, 3))
    normals = np.zeros_like(colours)
    work = {
        "ray_chart_plane_tests": count * len(baked),
        "rectangle_hits": 0,
        "nonzero_alpha_hits": 0,
    }
    for slot, data in enumerate(baked):
        chart = data["chart"]
        den = directions @ chart.basis[2]
        distance = np.divide(
            chart.offset - origins @ chart.basis[2],
            den,
            out=np.full(count, np.inf),
            where=den < 0,
        )
        valid = np.flatnonzero((den < 0) & (distance > 0.1) & np.isfinite(distance))
        points = origins[valid] + distance[valid, None] * directions[valid]
        uv = (points @ chart.basis[:2].T - chart.low) / (
            np.array(chart.shape[::-1]) * chart.pitch
        )
        inside = ((uv >= 0) & (uv <= 1)).all(1)
        ids = valid[inside]
        work["rectangle_hits"] += len(ids)
        values = bilinear(data["payload"], uv[inside])
        depths[ids, slot] = distance[ids]
        colours[ids, slot] = values[:, :3]
        alpha[ids, slot] = values[:, 3]
        normals[ids, slot] = np.divide(
            values[:, 4:],
            values[:, 3:4],
            out=np.zeros((len(ids), 3)),
            where=values[:, 3:4] > 0,
        )
        work["nonzero_alpha_hits"] += int((values[:, 3] > 0).sum())
    if prepared is not None:
        depth, _face, fields = sample(fallback, prepared, origins, directions)
        depths[:, -1] = depth
        alpha[:, -1] = np.isfinite(depth)
        colours[:, -1] = fields["colour"]
        normals[:, -1] = fields["normal"]
    depths[alpha == 0] = np.inf
    order = np.argsort(depths, axis=1, kind="stable")
    ordered_alpha = np.take_along_axis(alpha, order, axis=1)
    transmittance = np.cumprod(np.c_[np.ones(count), 1 - ordered_alpha], axis=1)[:, :-1]
    images = []
    for sun in suns:
        light = np.array(sun, float)
        light /= np.linalg.norm(light)
        brightness = 0.2 + 0.8 * np.maximum(normals @ light, 0)
        rgb = colours * brightness[:, :, None]
        rgb = np.take_along_axis(rgb, order[:, :, None], axis=1)
        images.append((transmittance[:, :, None] * rgb).sum(1))
    return depths.min(1), 1 - np.prod(1 - alpha, axis=1), np.asarray(images), work
