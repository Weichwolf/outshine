"""Complete fixed group templates and finite-surface displacement bounds, before views."""

import numpy as np
from outshine_ref.vegetation import FrondGuide


def transform(points, frame, origin):
    return sum(frame[:, k] * points[:, k, None] for k in range(3)) + origin


def cofactor(matrix):
    return np.stack(
        (
            np.cross(matrix[:, :, 1], matrix[:, :, 2]),
            np.cross(matrix[:, :, 2], matrix[:, :, 0]),
            np.cross(matrix[:, :, 0], matrix[:, :, 1]),
        ),
        axis=-1,
    )


def prepare(plan, source, coarse, transfer):
    vertices, normals, cofactors, faces, uv, records = [], [], [], [], [], []
    offset, face_offset = 0, 0
    for frond in plan["fronds"]:
        guide = FrondGuide.from_recipe(frond["guide"])
        root = np.asarray(frond["frame"])
        origin = np.asarray(frond["origin"])
        for pinna in frond["pinnae"]:
            frame = np.asarray(pinna["frame"])
            node = np.asarray(pinna["origin"])
            scale = pinna["length"]
            q = transform(coarse["vertices"] * scale, frame, node)
            position, jacobian = guide.evaluate(q)
            posed = transform(position, root, origin)
            cof = np.einsum("ij,njk->nik", root, cofactor(jacobian @ frame))
            n = cof[:, :, 2]
            n = n / np.linalg.norm(n, axis=1, keepdims=True)
            reference = source["vertices"][transfer["source_vertex_indices"]]
            reference = guide.evaluate(transform(reference * scale, frame, node))[0]
            reference = transform(reference, root, origin)
            a = np.einsum(
                "ni,nij->nj",
                transfer["source_weights"],
                reference[transfer["source_triangles"]],
            )
            b = np.einsum(
                "ni,nij->nj",
                transfer["coarse_weights"],
                posed[transfer["coarse_triangles"]],
            )
            error = float(np.linalg.norm(a - b, axis=1).max()) + 1e-10
            p = posed[coarse["faces"]]
            cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
            length = np.linalg.norm(cross, axis=1)
            if (
                np.any(length <= 0)
                or not (np.einsum("ij,ikj->ik", cross, n[coarse["faces"]]) > 0).all()
            ):
                raise ValueError("coarse field fails source orientation")
            records.append(
                {
                    "id": [frond["id"], pinna["id"]],
                    "first_vertex": offset,
                    "vertices": len(posed),
                    "first_face": face_offset,
                    "faces": len(coarse["faces"]),
                    "displacement_bound_m": error,
                    "source_pinnules": [p["id"] for p in plan["pinnules"]],
                }
            )
            vertices.append(posed)
            normals.append(n[coarse["faces"].ravel()])
            cofactors.append(cof)
            faces.append(coarse["faces"] + offset)
            uv.append(coarse["corner_uv"])
            offset += len(posed)
            face_offset += len(coarse["faces"])
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces).astype(np.int32),
        "normals": np.concatenate(normals),
        "corner_uv": np.concatenate(uv),
        "cofactor": np.concatenate(cofactors),
        "facet_normals": np.concatenate(
            [
                np.cross(
                    p[coarse["faces"]][:, 1] - p[coarse["faces"]][:, 0],
                    p[coarse["faces"]][:, 2] - p[coarse["faces"]][:, 0],
                )
                for p in vertices
            ]
        ),
    }, records


def group_mask(
    records,
    mesh,
    view_directions,
    world_scale,
    pixels_per_metre,
    max_error_px=0.75,
    min_cosine=0.4,
    angular_radii=None,
    projection_gains=None,
):
    directions = np.asarray(view_directions, float)
    if (
        directions.ndim != 2
        or directions.shape[1] != 3
        or not 1 <= len(directions) <= 8
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("bounded finite nonzero camera/light directions required")
    if (
        not 0 < world_scale <= 4
        or not 0 < pixels_per_metre <= 10000
        or not 0 <= max_error_px <= 4
        or not 0 < min_cosine < 1
    ):
        raise ValueError("bounded scale, pixel extent and selection limits required")
    radii = (
        np.zeros(len(directions))
        if angular_radii is None
        else np.asarray(angular_radii, float)
    )
    gains = (
        np.ones(len(directions))
        if projection_gains is None
        else np.asarray(projection_gains, float)
    )
    if (
        radii.shape != (len(directions),)
        or gains.shape != radii.shape
        or not np.isfinite((radii, gains)).all()
        or np.any((radii < 0) | (radii >= np.pi / 2))
        or np.any(gains < 1)
    ):
        raise ValueError("finite bounded direction cones and projection gains required")
    directions = directions / np.linalg.norm(directions, axis=1, keepdims=True)
    n = mesh["facet_normals"].copy()
    n /= np.linalg.norm(n, axis=1, keepdims=True)
    mask = []
    for record in records:
        first, end = record["first_face"], record["first_face"] + record["faces"]
        safe = (
            record["displacement_bound_m"]
            * world_scale
            * pixels_per_metre
            * gains.max()
            <= max_error_px
        )
        c = np.clip(abs(n[first:end] @ directions.T), 0, 1)
        minimum = c * np.cos(radii) - np.sqrt(np.maximum(0, 1 - c * c)) * np.sin(radii)
        mask.append(safe and (minimum >= min_cosine).all())
    return np.asarray(mask, bool)
