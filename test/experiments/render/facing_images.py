"""Near-clipped opaque diagnostic depth, retaining the original one-sided winding."""

import hashlib

import numpy as np
from current_visibility import raster
from PIL import Image
from static_facing import query

ENABLED = {
    "static": True,
    "one_sided": True,
    "trusted_winding": True,
    "model": np.eye(4),
}


def clip_near(triangles, near=0.1):
    inside = triangles[:, :, 2] >= near
    kept = list(triangles[inside.all(axis=1)])
    for triangle in triangles[inside.any(axis=1) & ~inside.all(axis=1)]:
        polygon = []
        previous = triangle[-1]
        for current in triangle:
            if (previous[2] >= near) != (current[2] >= near):
                ratio = (near - previous[2]) / (current[2] - previous[2])
                cut = previous + ratio * (current - previous)
                cut[2] = near
                polygon.append(cut)
            if current[2] >= near:
                polygon.append(current)
            previous = current
        for i in range(1, len(polygon) - 1):
            kept.append(np.array([polygon[0], polygon[i], polygon[i + 1]]))
    return np.array(kept).reshape(-1, 3, 3)


def render(vertices, indices, eye, basis):
    local = (vertices - eye) @ basis.T
    triangles = clip_near(local[indices])
    if not len(triangles):
        return np.zeros((720, 1280))
    points = triangles.reshape(-1, 3)
    projected = np.column_stack(
        (
            640 + 720 * points[:, 0] / points[:, 2],
            360 - 720 * points[:, 1] / points[:, 2],
            0.1 / points[:, 2],
        )
    )
    # Clipping has already accepted the near boundary. raster's z input is only
    # its front test; interpolation uses the unchanged projected reciprocal z.
    return raster(
        projected,
        np.maximum(points[:, 2], np.nextafter(0.1, np.inf)),
        np.arange(len(points)).reshape(-1, 3),
        1280,
        720,
    )


def image_comparison(model, records, output, *, selector=None):
    vertices, indices, scene = model["vertices"], model["indices"], model["scene"]
    target = vertices[np.unique(indices)].mean(axis=0)
    if scene is not None:
        eye = np.array(scene["eye_ecef"]) - scene["anchor_ecef"]
        up = np.array(scene["anchor_ecef"]) / np.linalg.norm(scene["anchor_ecef"])
    else:
        span = np.ptp(vertices, axis=0).max()
        eye = target + np.array([0, -span * 0.7, span * 0.3])
        up = np.array([0.0, 0, 1])
    toward = target - eye
    forward = toward / np.linalg.norm(toward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    upright = np.cross(right, forward)
    selections = {
        "original": np.ones(len(indices), bool),
        "cone": np.ones(len(indices), bool),
        "mask": np.ones(len(indices), bool),
    }
    if selector is None:
        for (first, count), record in zip(model["ranges"], records, strict=True):
            cone, mask = query(record, eye, **ENABLED)
            selections["cone"][first : first + count] = cone
            selections["mask"][first : first + count] = [
                bool(mask & (1 << i)) for i in range(count)
            ]
    else:
        selections = {"original": np.ones(len(indices), bool), **selector(eye)}
    cases = []
    tag = model["group"][:12]
    # Facing depends on eye position only. All 72 yaw poses therefore reuse the
    # same selection; four actual depth comparisons also check raster consumers.
    for yaw in (0, 90, 180, 270):
        angle = np.radians(yaw)
        direction = forward * np.cos(angle) + right * np.sin(angle)
        side = right * np.cos(angle) - forward * np.sin(angle)
        basis = np.stack((side, upright, direction))
        depths = []
        for name, keep in selections.items():
            depth = render(vertices, indices[keep], eye, basis)
            depths.append(depth)
            shade = (255 * np.sqrt(np.clip(depth * 50, 0, 1))).astype(np.uint8)
            Image.fromarray(shade).save(output / f"{tag}-{yaw}-{name}.png")
        if any(not np.array_equal(depths[0], depth) for depth in depths[1:]):
            raise ValueError("near-clipped depth changed after facing selection")
        cases.append(
            {
                "group": model["group"],
                "model": model["key"],
                "yaw_deg": yaw,
                "covered_pixels": int(np.count_nonzero(depths[0])),
                "changed_depth_pixels": 0,
                "depth_sha256": hashlib.sha256(depths[0].tobytes()).hexdigest(),
                "fixed_eye_full_rotation_yaw_poses": list(range(0, 360, 5)),
                "rotation_scope": "selection invariant at fixed eye; four raster views",
                "selected_index_references": {
                    k: 3 * int(v.sum()) for k, v in selections.items()
                },
            }
        )
    if not cases[0]["covered_pixels"]:
        raise ValueError("empty forward comparison cannot establish preservation")
    return cases
