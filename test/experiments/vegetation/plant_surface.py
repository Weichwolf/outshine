"""Complete wood unions from canonical plant axes; no implicit global volume."""

import manifold3d as manifold
import numpy as np
from axis_surface import axis_mesh


def axis_products(plant, segments=24, sides=8, max_axes=4096):
    if (
        type(max_axes) is not int
        or not 1 <= max_axes <= 4096
        or type(segments) is not int
        or not 2 <= segments <= 1024
        or type(sides) is not int
        or not 3 <= sides <= 128
    ):
        raise ValueError("bounded integer mesh and axis budgets required")
    count = sum(plant.axis_counts)
    if count > max_axes or count * (segments + 1) * sides * 2 > 1_000_000:
        raise ValueError("complete wood request exceeds its axis/triangle budget")
    products = []
    for path in plant.paths():
        state = plant.axis(path)
        level = plant.levels[len(path)]
        mesh = axis_mesh(
            state.length,
            state.bend,
            level.base_radius,
            level.tip_radius,
            segments=segments,
            sides=sides,
        )
        products.append(
            {
                "vertices": mesh["vertices"] @ state.frame.T + state.origin,
                "faces": mesh["faces"],
                "normals": mesh["normals"] @ state.frame.T,
            }
        )
    return products


def solid_from_mesh(mesh):
    vertices = np.array(mesh["vertices"], dtype=np.float64, order="C", copy=True)
    faces = np.array(mesh["faces"], dtype=np.uint64, order="C", copy=True)
    if "normals" in mesh:
        indices = faces.ravel()
        _, first, inverse = np.unique(indices, return_index=True, return_inverse=True)
        targets = first[inverse]
        sources = np.flatnonzero(targets != np.arange(len(indices)))
        properties = np.column_stack((vertices[indices], mesh["normals"]))
        raw = manifold.Mesh64(
            properties,
            np.arange(len(indices), dtype=np.uint64).reshape(-1, 3),
            sources.astype(np.uint64),
            targets[sources].astype(np.uint64),
        )
    else:
        raw = manifold.Mesh64(vertices, faces)
    solid = manifold.Manifold(raw)
    if solid.status() != manifold.Error.NoError:
        raise ValueError(f"invalid source solid: {solid.status()}")
    return solid


def union_mesh(products, method="batch"):
    if not products or method not in ("batch", "sequential"):
        raise ValueError("nonempty mesh inputs and declared union method required")
    if len(products) > 4096 or sum(len(m["faces"]) for m in products) > 1_000_000:
        raise ValueError("bounded complete union inputs required")
    solids = [solid_from_mesh(mesh) for mesh in products]
    if method == "batch":
        result = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
    else:
        result = solids[0]
        for solid in solids[1:]:
            result = result + solid
            if result.status() != manifold.Error.NoError:
                raise ValueError("intermediate union failed")
    if result.status() != manifold.Error.NoError or result.is_empty():
        raise ValueError("complete union failed")
    result = result.simplify(1e-8)
    if result.num_prop() < 3:
        result = result.calculate_normals(0, 60)
    raw = result.to_mesh64()
    properties = np.asarray(raw.vert_properties)
    faces = np.asarray(raw.tri_verts, dtype=np.int64)
    representatives = np.arange(len(properties))
    representatives[np.asarray(raw.merge_from_vert, dtype=int)] = np.asarray(
        raw.merge_to_vert, dtype=int
    )
    for _ in range(len(properties)):
        compressed = representatives[representatives]
        if np.array_equal(compressed, representatives):
            break
        representatives = compressed
    else:
        raise ValueError("cyclic mesh merge map")
    unique, inverse = np.unique(representatives, return_inverse=True)
    normals = properties[faces, 3:6].reshape(-1, 3).copy()
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    return {
        "vertices": properties[unique, :3].copy(),
        "faces": inverse[faces].astype(np.int32),
        "normals": normals,
    }
