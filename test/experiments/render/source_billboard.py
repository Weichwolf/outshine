"""Two-triangle source-group cards against a shared triangle-image reference.

Reference and relightable cards share native colour-factor/Lambert evaluation.
This isolates representation errors; it is not native PBR/material acceptance.
"""

import numpy as np
import trimesh
from source_far_reprojection import intersect_source, prepare_source
from source_surface_fields import relight, surface_fields


def join_sources(sources):
    positions, indices, colour, offset = [], [], [], 0
    for source in sources:
        positions.append(source["positions"])
        indices.append(source["indices"].astype(np.int64) + offset)
        colour.append(source["palette"][source["colour_index"]])
        offset += len(source["positions"])
    palette, ids = np.unique(np.concatenate(colour), axis=0, return_inverse=True)
    return {
        "positions": np.concatenate(positions),
        "indices": np.concatenate(indices),
        "normal": np.concatenate([s["normal"] for s in sources]),
        "code": np.concatenate([s["code"] for s in sources]),
        "palette": palette,
        "colour_index": ids,
        "triangle_source": np.repeat(
            np.arange(len(sources)), [len(s["indices"]) for s in sources]
        ),
        "members": [s["metadata"] for s in sources],
        "native_bytes": sum(s["native_bytes"] for s in sources),
    }


def projected_bounds(positions, view):
    eye, basis, focal, width, height = view
    p = (positions.astype(float) - eye) @ basis.T
    if (p[:, 2] <= 0.1).any():
        raise ValueError("bounded diagnostic requires geometry in front of near plane")
    xy = [width / 2, height / 2] + focal * p[:, :2] / p[:, 2, None] * [1, -1]
    return np.stack((xy.min(0) - 2, xy.max(0) + 2))


def image_rays(view, box, width, height, supersample=2):
    eye, basis, focal, screen_width, screen_height = view
    x = box[0, 0] + (np.arange(width * supersample) + 0.5) / (
        width * supersample
    ) * np.ptp(box[:, 0])
    y = box[0, 1] + (np.arange(height * supersample) + 0.5) / (
        height * supersample
    ) * np.ptp(box[:, 1])
    xx, yy = np.meshgrid(x, y)
    direction = (
        np.column_stack(
            (
                (xx.ravel() - screen_width / 2) / focal,
                -(yy.ravel() - screen_height / 2) / focal,
                np.ones(xx.size),
            )
        )
        @ basis
    )
    return np.broadcast_to(eye, direction.shape), direction


def average_samples(values, width, height, supersample):
    return values.reshape(height, supersample, width, supersample, -1).mean((1, 3))


def reference_samples(source, prepared, origins, directions):
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    fields = surface_fields(source, faces)
    return depth, fields


def bake_card(source, prepared, view, sun, *, texture_size=64, supersample=2):
    if not 8 <= texture_size <= 2048 or supersample not in (1, 2, 4):
        raise ValueError("bounded texture and sample dimensions required")
    box = projected_bounds(source["positions"], view)
    extent = box[1] - box[0]
    width, height = np.maximum(np.ceil(texture_size * extent / extent.max()), 1).astype(
        int
    )
    origins, directions = image_rays(view, box, width, height, supersample)
    depth, fields = reference_samples(source, prepared, origins, directions)
    alpha = np.isfinite(depth).astype(float)
    colour = average_samples(
        fields["colour"] * alpha[:, None], width, height, supersample
    )
    normal = average_samples(
        fields["normal"] * alpha[:, None], width, height, supersample
    )
    a = average_samples(alpha, width, height, supersample)[:, :, 0]
    base = np.divide(
        colour, a[:, :, None], out=np.zeros_like(colour), where=a[:, :, None] > 0
    )
    # Keep prefiltered normal moments: normalisation after filtering would invent
    # a new unit normal. The directional Lambert approximation remains explicit.
    moment = np.divide(
        normal, a[:, :, None], out=np.zeros_like(normal), where=a[:, :, None] > 0
    )
    matte = average_samples(relight(fields, sun, depth), width, height, supersample)
    rgba = np.dstack((base, a))
    rgba = np.floor(np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8)
    # Float16 moments are unlit data, independent of the baked matte's sun.
    moment = np.dstack((moment, np.zeros_like(a))).astype(np.float16)
    matte = np.divide(
        matte, a[:, :, None], out=np.zeros_like(matte), where=a[:, :, None] > 0
    )
    matte = np.floor(np.clip(np.dstack((matte, a)), 0, 1) * 255 + 0.5).astype(np.uint8)
    eye, basis, focal, screen_width, screen_height = view
    centre = (source["positions"].min(0).astype(float) + source["positions"].max(0)) / 2
    z = float((centre - eye) @ basis[2])
    pixels = np.array(
        [
            [box[0, 0], box[0, 1]],
            [box[1, 0], box[0, 1]],
            [box[1, 0], box[1, 1]],
            [box[0, 0], box[1, 1]],
        ]
    )
    local = np.column_stack(
        (
            (pixels[:, 0] - screen_width / 2) / focal * z,
            -(pixels[:, 1] - screen_height / 2) / focal * z,
            np.full(4, z),
        )
    )
    # Screen XY ordering gives front-facing geometric winding in world space.
    mesh = {
        "positions": (eye + local @ basis).astype(np.float32),
        "indices": np.array([[0, 2, 1], [0, 3, 2]], np.uint16),
    }
    return {
        "mesh": mesh,
        "uv": np.array([[0, 0], [1, 0], [1, 1], [0, 1.0]], np.float32),
        "rgba": rgba,
        "normal_moment": moment,
        "matte": matte,
        "box": box,
        "view": view,
        "source_rays": len(origins),
        "members": source["members"],
        "prepared": prepare_source(mesh),
    }


def bilinear(image, uv):
    height, width = image.shape[:2]
    p = uv * [width, height] - 0.5
    first = np.floor(p).astype(int)
    fraction = p - first
    result = np.zeros((len(uv), image.shape[2]))
    for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
        index = first + [x, y]
        weight = (fraction[:, 0] if x else 1 - fraction[:, 0]) * (
            fraction[:, 1] if y else 1 - fraction[:, 1]
        )
        inside = ((index >= 0) & (index < [width, height])).all(1)
        result[inside] += (
            image[index[inside, 1], index[inside, 0]] * weight[inside, None]
        )
    return result


def card_samples(card, origins, directions, sun):
    depth, faces = intersect_source(
        card["mesh"], origins, directions, prepared=card["prepared"]
    )
    valid = faces >= 0
    points = origins[valid] + depth[valid, None] * directions[valid]
    triangles = card["mesh"]["positions"][card["mesh"]["indices"][faces[valid]]].astype(
        float
    )
    bary = trimesh.triangles.points_to_barycentric(triangles, points)
    uv = np.einsum(
        "ni,nij->nj", bary, card["uv"][card["mesh"]["indices"][faces[valid]]]
    )
    rgba = card["rgba"].astype(float) / 255
    # Filter premultiplied data so transparent texels create no dark fringes.
    payload = np.concatenate(
        (
            rgba[:, :, :3] * rgba[:, :, 3:4],
            rgba[:, :, 3:4],
            card["normal_moment"][:, :, :3].astype(float) * rgba[:, :, 3:4],
        ),
        axis=2,
    )
    filtered = bilinear(payload, uv)
    alpha = np.zeros(len(origins))
    alpha[valid] = filtered[:, 3]
    normal = np.divide(
        filtered[:, 4:],
        filtered[:, 3:4],
        out=np.zeros_like(filtered[:, 4:]),
        where=filtered[:, 3:4] > 0,
    )
    colour = np.divide(
        filtered[:, :3],
        filtered[:, 3:4],
        out=np.zeros_like(filtered[:, :3]),
        where=filtered[:, 3:4] > 0,
    )
    rgb = np.zeros((len(origins), 3))
    rgb[valid] = (
        relight({"normal": normal, "colour": colour}, sun, depth[valid])
        * filtered[:, 3:4]
    )
    matte = np.zeros_like(rgb)
    texture = card["matte"].astype(float) / 255
    matte[valid] = bilinear(texture[:, :, :3] * texture[:, :, 3:4], uv)
    return depth, alpha, matte, rgb
