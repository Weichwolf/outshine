"""Forward-project bounded depth-sample supports without original geometry.

Each captured sample owns its half-pixel square in the capture image. A tangent
patch is depth-tested in the novel view. This proposal can extend across source
edges and cannot contain unseen surfaces; failures are measured before fallback.
"""

import numpy as np
from source_billboard import image_rays


def raster(cache, prepared, view, lo, width, height):
    eye, basis, focal, screen_width, screen_height = view
    _, directions = image_rays(
        view, np.array([lo, lo + [width, height]]), width, height, 2
    )
    result = np.full(len(directions), np.inf)
    slots = np.full(len(directions), -1, np.int32)
    local = (prepared["corners"] - eye) @ basis.T
    front = np.einsum("ni,ni->n", prepared["normal"], prepared["point"] - eye) < -1e-12
    valid = prepared["valid"] & front & (local[:, :, 2] > 0.1).all(1)
    candidates = np.flatnonzero(valid)
    tested = accepted = updates = 0
    cap_eye, cap_basis, cap_focal, cap_width, cap_height = cache["view"]
    capture_width = cache["width"] * cache["supersample"]
    for i in candidates:
        p = local[i]
        screen = [screen_width / 2, screen_height / 2] + focal * p[:, :2] / p[
            :, 2, None
        ] * [1, -1]
        a = np.maximum(np.floor((screen.min(0) - lo) * 2).astype(int) - 1, 0)
        b = np.minimum(
            np.ceil((screen.max(0) - lo) * 2).astype(int) + 1, [width * 2, height * 2]
        )
        if (a >= b).any():
            continue
        y, x = np.mgrid[a[1] : b[1], a[0] : b[0]]
        ids = (y * width * 2 + x).ravel()
        d, normal = directions[ids], prepared["normal"][i]
        denominator = d @ normal
        t = np.divide(
            (prepared["point"][i] - eye) @ normal,
            denominator,
            out=np.full(len(ids), np.inf),
            where=denominator < -1e-12,
        )
        finite = np.isfinite(t) & (t > 0.1)
        points = eye + np.where(finite, t, 0)[:, None] * d
        cap = (points - cap_eye) @ cap_basis.T
        projected = np.divide(
            cap[:, :2],
            cap[:, 2:3],
            out=np.zeros((len(ids), 2)),
            where=cap[:, 2:3] > 0.1,
        )
        xy = (
            [cap_width / 2, cap_height / 2]
            + cap_focal * projected * [1, -1]
            - cache["lo"]
        ) * cache["supersample"]
        pixel = cache["data"]["pixel"][i]
        begin = [pixel % capture_width, pixel // capture_width]
        inside = (
            finite
            & (cap[:, 2] > 0.1)
            & (xy >= begin).all(1)
            & (xy < np.array(begin) + cache["data"]["span"][i]).all(1)
        )
        replace = inside & (t < result[ids])
        result[ids[replace]], slots[ids[replace]] = t[replace], i
        tested += len(ids)
        accepted += int(inside.sum())
        updates += int(replace.sum())
    return (
        result,
        slots,
        {
            "all_samples": len(cache["data"]),
            "front_supported_samples": len(candidates),
            "unsupported_capture_samples": int((~prepared["valid"]).sum()),
            "near_or_back_rejected_samples": int((prepared["valid"] & ~valid).sum()),
            "nominal_triangle_equivalents": len(candidates) * 2,
            "candidate_subpixel_pairs": tested,
            "accepted_support_samples": accepted,
            "depth_updates": updates,
            "result_and_ray_array_bytes": result.nbytes
            + slots.nbytes
            + directions.nbytes,
            "scope": "Python forward support projection/plane tests, not native GPU work or physical memory traffic. Partial near-plane supports are unsupported, not silently clipped/accepted. Per-patch temporaries/library allocations not fully counted.",
        },
    )
