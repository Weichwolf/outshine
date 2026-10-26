"""Reference work factors and pre-depth projected primitive boxes; no GPU timings."""

import numpy as np
from current_visibility import project


def factor_work(original, opaque, fractional):
    a, b = original["hit_samples"], opaque["hit_samples"]
    ga, gb, gf = (
        work["ggx_lobe_evaluations"] for work in (original, opaque, fractional)
    )
    if min(a, b, ga, gb, gf) <= 0:
        return {
            "scope": "Positive reference hit/GGX counts required for factorization."
        }
    return {
        "proxy_first_hit_sample_factor": b / a,
        "first_hit_average_lobe_factor": (gb / b) / (ga / a),
        "additional_layers_ggx_factor": gf / gb,
        "total_reference_ggx_factor": gf / ga,
        "opaque_proxy_fraction_of_full_ggx": gb / gf,
        "scope": "Algebraic work factors only; changed geometry/normal/coverage distributions are coupled. Not additive causal fractions or GPU times.",
    }


def projected_work(mesh, camera, maximum_face_lobes):
    positions, forward = project(mesh["positions"], camera)
    lobes = np.asarray(maximum_face_lobes)
    if (
        np.any(forward <= 0.1)
        or lobes.shape != (len(mesh["indices"]),)
        or lobes.dtype.kind not in "iu"
        or np.any(lobes < 0)
        or np.any(lobes > 16)
    ):
        raise ValueError("fully far geometry and one lobe bound per triangle required")
    triangle = positions[mesh["indices"], :2]
    a, b = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    front = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] < 0
    triangle = triangle[front]
    if not len(triangle):
        pixels = quads = np.empty(0, np.int64)
    else:
        low = np.clip(np.floor(triangle.min(axis=1)), [0, 0], camera[3:]).astype(
            np.int64
        )
        high = np.clip(np.ceil(triangle.max(axis=1)), [0, 0], camera[3:]).astype(
            np.int64
        )
        pixels = np.prod(np.maximum(high - low, 0), axis=1)
        quads = np.prod(np.maximum((high + 1) // 2 - low // 2, 0), axis=1)
        quads[pixels == 0] = 0
    return {
        "submitted_vertices": len(mesh["positions"]),
        "referenced_unique_vertices": len(np.unique(mesh["indices"])),
        "index_references": mesh["indices"].size,
        "submitted_triangles": len(mesh["indices"]),
        "front_facing_reference_triangles": int(front.sum()),
        "reference_position_array_bytes": mesh["positions"].nbytes,
        "reference_index_array_bytes": mesh["indices"].nbytes,
        "reference_position_dtype": str(mesh["positions"].dtype),
        "reference_index_dtype": str(mesh["indices"].dtype),
        "triangle_bbox_pixel_slots_before_depth": int(pixels.sum()),
        "triangle_bbox_2x2_quad_slots_before_depth": int(4 * quads.sum()),
        "logical_lobe_quad_slots_before_depth": int(4 * (quads * lobes[front]).sum()),
        "scope": "Double reference projection, front-face triangle boxes clipped to viewport, before depth/sample coverage. Conservative box counts for this reference, not actual covered fragments, native interpolated-normal shading, hardware quad invocations or GPU times. Material lobe maxima are a logical response model; no division of SS16 counts into native pixel cost.",
    }


def candidate_face_lobes(original, fields, product, appearance, mesh):
    count = len(product["indices"])
    triangles = mesh["positions"].astype(float)[mesh["indices"][:count]]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    role = (normal[:, 2] > 0.5).astype(int)
    grouped = np.count_nonzero(
        appearance["roughness_weights"][product["triangle_groups"], role], axis=1
    )
    retained = np.isin(original["binding"][:, 1], product["needs_sources"])
    needs = np.count_nonzero(fields["weights"][retained], axis=-1).max(axis=1)
    return np.r_[grouped, needs]
