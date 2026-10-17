"""Projected source-area quadrature; reuse actual same-source camera/light rays."""

import numpy as np
from needle_joint_contacts import direct_response
from needle_response import light_direction
from outshine_ref.vegetation.quadrature import quadrature
from surface_area_cells import clipped_batches
from surface_normal_transfer import interpolate_normals


def triangle_rule(order):
    if type(order) is not int or order not in (1, 2, 4, 6, 8):
        raise ValueError("explicit bounded triangle quadrature order1/2/4/6/8 required")
    if order == 1:
        return np.array([[1 / 3, 1 / 3, 1 / 3]]), np.ones(1)
    nodes, weights = quadrature(order)
    axis = (nodes + 1) / 2
    weights = weights / 2
    r, s = np.meshgrid(axis, axis, indexing="ij")
    w = 2 * r * np.outer(weights, weights)
    return np.column_stack(
        (1 - r.ravel(), (r * (1 - s)).ravel(), (r * s).ravel())
    ), w.ravel()


def primary_visible(source, scene, points, face_ids, view, bias, two_sided):
    triangles = source["vertices"][source["faces"][face_ids]]
    geometric = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    geometric /= np.linalg.norm(geometric, axis=1)[:, None]
    if two_sided:
        geometric *= np.where(geometric @ view < 0, -1, 1)[:, None]
    return direct_response(scene, {"points": points, "normals": geometric}, view, bias)[
        1
    ]


def integrate(
    source,
    scene,
    view,
    lights,
    size=16,
    order=2,
    max_samples=2_000_000,
    *,
    bias=2e-6,
    pixel_mask=None,
    normal_measure=None,
    contact_measure=None,
    two_sided=False,
):
    if type(two_sided) is not bool:
        raise ValueError("explicit surface sidedness required")
    if type(size) is not int or size not in (1, 4, 16, 64) or not 1 <= len(lights) <= 8:
        raise ValueError("bounded native footprints and one to eight lights required")
    if (
        scene.index.vertices is not source["vertices"]
        or scene.index.faces is not source["faces"]
    ):
        raise ValueError("same prepared immutable source and ray geometry required")
    view = light_direction(view)
    if normal_measure is not None and normal_measure.size != size:
        raise ValueError("same-grid visible normal measure required")
    if not np.isfinite(bias) or not 1e-7 <= bias <= 1e-4:
        raise ValueError("explicit metric source-ray bias1e-7..1e-4 required")
    if pixel_mask is not None and (
        not isinstance(pixel_mask, np.ndarray)
        or pixel_mask.dtype != np.bool_
        or pixel_mask.shape != (size, size)
    ):
        raise ValueError("explicit same-grid boolean pixel demand required")
    lights = np.array([light_direction(direction) for direction in lights])
    if contact_measure is not None:
        contact_measure.check(source, scene, view, size, bias, two_sided=two_sided)
    if (
        not 1 <= len(lights) <= 8
        or type(max_samples) is not int
        or not 1 <= max_samples <= 2_000_000
    ):
        raise ValueError(
            "one to eight lights and explicit bounded sample work required"
        )
    bary, weights = triangle_rule(order)
    output = np.zeros((1 + 2 * len(lights), size * size))
    work = {
        "sampled_points": 0,
        "camera_visible_points": 0,
        "maximum_sample_batch": 0,
        "reused_coaxial_positive_contacts": 0,
        "requested_pixels": size * size
        if pixel_mask is None
        else int(pixel_mask.sum()),
        "complete_footprint": pixel_mask is None or bool(pixel_mask.all()),
    }
    for triangles, ids, pixels, area in clipped_batches(
        source, view, size, work, pixel_mask=pixel_mask, two_sided=two_sided
    ):
        if work["sampled_points"] + len(triangles) * len(weights) > max_samples:
            raise ValueError(
                "source-area work budget exceeded before quadrature expansion"
            )
        for first in range(0, len(triangles), 8192 // len(weights)):
            last = min(first + 8192 // len(weights), len(triangles))
            points = np.einsum("ki,nij->nkj", bary, triangles[first:last]).reshape(
                -1, 3
            )
            face_ids = np.repeat(ids[first:last], len(weights))
            slots = np.repeat(pixels[first:last], len(weights))
            mass = (area[first:last, None] * weights).ravel()
            visible = primary_visible(
                source, scene, points, face_ids, view, bias, two_sided
            )
            work["sampled_points"] += len(points)
            work["maximum_sample_batch"] = max(
                work["maximum_sample_batch"], len(points)
            )
            work["camera_visible_points"] += int(visible.sum())
            points, face_ids, slots, mass = (
                value[visible] for value in (points, face_ids, slots, mass)
            )
            if not len(points):
                continue
            output[0] += np.bincount(slots, weights=mass, minlength=size * size)
            normals = interpolate_normals(source, points, face_ids)
            if two_sided:
                normals *= np.where(normals @ view < 0, -1, 1)[:, None]
            if normal_measure is not None:
                normal_measure.add(slots, mass, normals)
            if contact_measure is not None:
                contact_measure.add(points, slots, mass, normals, face_ids=face_ids)
            for index, direction in enumerate(lights):
                if np.array_equal(direction, view):
                    cosine = np.maximum(normals @ direction, 0)
                    clear = np.ones(len(points), bool)
                    work["reused_coaxial_positive_contacts"] += int(
                        np.count_nonzero(cosine > 0)
                    )
                else:
                    cosine, clear, _ = direct_response(
                        scene, {"points": points, "normals": normals}, direction, bias
                    )
                output[1 + 2 * index] += np.bincount(
                    slots, weights=mass * cosine, minlength=size * size
                )
                output[2 + 2 * index] += np.bincount(
                    slots, weights=mass * cosine * clear, minlength=size * size
                )
    return output.reshape(-1, size, size), work
