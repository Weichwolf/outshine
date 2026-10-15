"""Boundary-preserving finite leaf meshes; no atlas, new shape samples or RNG."""

import numpy as np
import shapely
from fern_mesh_ops import combine
from pinna_surface import domain, overlay


def part(mesh, face_mask):
    faces = mesh["faces"][face_mask]
    used, inverse = np.unique(faces, return_inverse=True)
    return {
        "vertices": mesh["vertices"][used].copy(),
        "faces": inverse.reshape(-1, 3).astype(np.int32),
        "normals": mesh["normals"].reshape(-1, 3, 3)[face_mask].reshape(-1, 3).copy(),
        "corner_uv": mesh["corner_uv"]
        .reshape(-1, 3, 2)[face_mask]
        .reshape(-1, 2)
        .copy(),
        "organ_ids": mesh["organ_ids"][used].copy(),
        "surface": mesh["surface"][face_mask].copy(),
    }


def _leaf(mesh):
    vertices, faces = mesh["vertices"], mesh["faces"]
    polygon = domain(vertices, faces)
    if polygon.geom_type != "Polygon" or len(polygon.interiors):
        raise ValueError("one simple projected leaf without holes required")
    # GEOS uses boundary vertices only. Preserve their original 3D coordinates.
    raw = shapely.get_coordinates(
        shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
    ).reshape(-1, 4, 2)[:, :3]
    lookup = {tuple(p): i for i, p in enumerate(vertices[:, :2])}
    if len(lookup) != len(vertices):
        raise ValueError("unique leaf projection vertices required")
    try:
        new_faces = np.array([lookup[tuple(p)] for p in raw.reshape(-1, 2)])
    except KeyError as error:
        raise ValueError("triangulation inserted a non-source vertex") from error
    new_faces = new_faces.reshape(-1, 3)
    p = vertices[new_faces]
    normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    new_faces[normal[:, 2] < 0] = new_faces[normal[:, 2] < 0, ::-1]
    attributes = []
    for name, width in (("normals", 3), ("corner_uv", 2)):
        corner = mesh[name].reshape(-1, width)
        values = np.empty((len(vertices), width))
        for v in range(len(vertices)):
            a = corner[faces.ravel() == v]
            if not np.allclose(a, a[0], rtol=0, atol=1e-12):
                raise ValueError("hard corner attributes require original topology")
            values[v] = a[0]
        attributes.append(values[new_faces.ravel()])
    p = vertices[new_faces]
    cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    if not (np.einsum("ij,ikj->ik", cross, attributes[0].reshape(-1, 3, 3)) > 0).all():
        raise ValueError("boundary triangulation reverses the source normal field")
    used, inverse = np.unique(new_faces, return_inverse=True)
    result = {
        "vertices": vertices[used].copy(),
        "faces": inverse.reshape(-1, 3).astype(np.int32),
        "normals": attributes[0],
        "corner_uv": attributes[1],
        "organ_ids": mesh["organ_ids"][used].copy(),
        "surface": np.full(len(new_faces), 6, np.uint8),
    }
    if not domain(result["vertices"], result["faces"]).equals(polygon):
        raise ValueError("leaf domain changed")
    return result


def boundary_product(mesh):
    """One canonical pinna, smooth corner fields; secondary rib stays untouched."""
    if mesh["organ_ids"].shape != (len(mesh["vertices"]), 1):
        raise ValueError("canonical single-level organ identities required")
    leaf_faces = mesh["surface"] == 6
    ids = mesh["organ_ids"][mesh["faces"][:, 0], 0]
    parts = [
        _leaf(part(mesh, leaf_faces & (ids == i))) for i in np.unique(ids[leaf_faces])
    ]
    if np.any(~leaf_faces):
        parts.append(part(mesh, ~leaf_faces))
    return combine(parts)


def correspondence(source, coarse):
    """Common XY overlays per leaf, excluding unrelated overlapping neighbours."""
    result = {
        key: []
        for key in (
            "source_triangles",
            "coarse_triangles",
            "source_weights",
            "coarse_weights",
        )
    }
    si = source["organ_ids"][source["faces"][:, 0], 0]
    ci = coarse["organ_ids"][coarse["faces"][:, 0], 0]
    for identity in np.unique(si[source["surface"] == 6]):
        sm = (si == identity) & (source["surface"] == 6)
        cm = (ci == identity) & (coarse["surface"] == 6)
        a, b = part(source, sm), part(coarse, cm)
        if not domain(a["vertices"], a["faces"]).equals(
            domain(b["vertices"], b["faces"])
        ):
            raise ValueError("matching leaf domains required")
        transfer = overlay(a, b)
        for key, destination in result.items():
            value = transfer[key]
            if key == "source_triangles":
                value = np.unique(source["faces"][sm])[value]
            if key == "coarse_triangles":
                value = np.unique(coarse["faces"][cm])[value]
            destination.append(value)
    return {key: np.concatenate(value) for key, value in result.items()}


def group_bounds(source, coarse, transfer):
    """Finite 3D surface displacement after the actual cached guide deformation.

    Both meshes are affine on each XY overlay polygon. The Euclidean norm of
    their difference is convex there, so its maximum is at an overlay vertex.
    This does not bound normal, radiance, coverage area or arbitrary shadows.
    """
    records = []
    si = source["organ_ids"][source["faces"][:, 0], :2]
    ci = coarse["organ_ids"][coarse["faces"][:, 0], :2]
    groups = np.unique(si[source["surface"] == 6], axis=0)
    for identity in groups:
        sm, cm = np.all(si == identity, axis=1), np.all(ci == identity, axis=1)
        a = source["vertices"][np.unique(source["faces"][sm])]
        b = coarse["vertices"][np.unique(coarse["faces"][cm])]
        x = np.einsum(
            "ni,nij->nj", transfer["source_weights"], a[transfer["source_triangles"]]
        )
        y = np.einsum(
            "ni,nij->nj", transfer["coarse_weights"], b[transfer["coarse_triangles"]]
        )
        records.append(
            {
                "id": identity.tolist(),
                "displacement_bound_m": float(np.linalg.norm(x - y, axis=1).max())
                + 1e-10,
            }
        )
    return records


def select(
    records, world_scale, pixels_per_metre, projection_gain=1, max_error_px=0.75
):
    if not np.isfinite(
        (world_scale, pixels_per_metre, projection_gain, max_error_px)
    ).all() or not (
        0 < world_scale <= 4
        and 0 < pixels_per_metre <= 10000
        and projection_gain >= 1
        and 0 <= max_error_px <= 4
    ):
        raise ValueError(
            "finite bounded scale and explicit projection/error budget required"
        )
    return {
        tuple(r["id"])
        for r in records
        if r["displacement_bound_m"] * world_scale * pixels_per_metre * projection_gain
        <= max_error_px
    }


def snapshot(source, coarse, selected):
    known = {
        tuple(v)
        for v in source["organ_ids"][source["faces"][source["surface"] == 6, 0], :2]
    }
    if not selected <= known:
        raise ValueError("known pinna identities required")
    masks = []
    for mesh in (source, coarse):
        ids = mesh["organ_ids"][mesh["faces"][:, 0], :2]
        use = np.array([tuple(i) in selected for i in ids]) & (mesh["surface"] == 6)
        masks.append(use)
    if not selected:
        return part(source, np.ones(len(source["faces"]), bool))
    return combine([part(source, ~masks[0]), part(coarse, masks[1])])
