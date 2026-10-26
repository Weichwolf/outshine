"""Triangulate ready analytic solids only for independent lighting diagnostics."""

import numpy as np
from packed_normals import unpack
from roof_prism_control import explicit_mesh


def represented_mesh(product):
    prisms, _materials, _hierarchy, fallback, _prepared = product
    triangles, normals, owners, sources, planes = [], [], [], [], []
    maximum_residual = 0.0
    for row, (begin, end) in enumerate(
        zip(prisms.offsets[:-1], prisms.offsets[1:], strict=True)
    ):
        equations = prisms.planes[begin:end]
        centre = (
            prisms.bounds[row].mean(0)
            if len(prisms.bounds)
            else _materials.frames[row, 0]
        )
        xyz = explicit_mesh(equations, centre)
        local = xyz - centre
        distance = equations[:, 3] - equations[:, :3] @ centre
        residual = np.abs(
            np.einsum("tvi,pi->tvp", local, equations[:, :3]) - distance
        ).max(1)
        geometric = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        outward = geometric @ equations[:, :3].T > 0
        matches = (residual <= 1e-7) & outward
        if not matches.any(1).all():
            raise ValueError("Diagnostic hull face has no represented plane")
        face = matches.argmax(1)
        maximum_residual = max(
            maximum_residual, float(residual[np.arange(len(face)), face].max())
        )
        triangles.append(xyz)
        normals.append(np.repeat(equations[face, :3], 3, axis=0))
        sources.extend([int(prisms.binding[row, 0])] * len(xyz))
        owners.extend([row] * len(xyz))
        planes.extend((begin + face).tolist())
    xyz = np.concatenate(triangles) if triangles else np.empty((0, 3, 3))
    normal = np.concatenate(normals) if normals else np.empty((0, 3))
    vertices = xyz.reshape(-1, 3)
    indices = np.arange(len(vertices), dtype=np.uint32).reshape(-1, 3)
    colours = np.ones((len(vertices), 4), np.float32)
    if len(fallback["indices"]):
        indices = np.concatenate((indices, fallback["indices"] + len(vertices)))
        vertices = np.concatenate((vertices, fallback["positions"].astype(float)))
        normal = np.concatenate((normal, unpack(fallback["normal"])))
        colours = np.concatenate((colours, fallback["colour"]))
        source_ids = (
            fallback["binding"][:, 1]
            if "binding" in fallback
            else fallback["source_id"]
        )
        sources.extend(source_ids.tolist())
        owners.extend([-1] * len(fallback["indices"]))
        planes.extend([-1] * len(fallback["indices"]))
    return {
        "positions": vertices,
        "indices": indices,
        "shading_normal": normal,
        "colour": colours,
        "source_id": np.array(sources, np.int32),
        "primitive_owner": np.array(owners, np.int32),
        "plane_owner": np.array(planes, np.int32),
    }, maximum_residual


def write_scene(output, mesh, view, basis, centre, ground, producer):
    import json

    output.mkdir(parents=True, exist_ok=False)
    arrays = dict(mesh)
    arrays["positions"] = ((mesh["positions"].astype(float) - centre) @ basis.T).astype(
        np.float32
    )
    arrays["shading_normal"] = (mesh["shading_normal"] @ basis.T).astype(np.float32)
    np.savez_compressed(output / "mesh.npz", **arrays)
    eye, camera, focal, width, height = view
    scene = {
        "producer": producer,
        "mesh": "mesh.npz",
        "eye": ((eye - centre) @ basis.T).tolist(),
        "camera_basis": (camera @ basis.T).tolist(),
        "focal_px": focal,
        "width": width,
        "height": height,
        "ground": ground,
        "scope": "Ready attributed-prism diagnostic polyhedra versus complete original, neutral opaque metallic-roughness PBR. Ready product is built/reloaded before diagnostic triangulation. UV/metres/material programs are independently queried, not implemented in this shared neutral shader. Not a native raster/filter/caster/device admission.",
    }
    (output / "scene.json").write_text(json.dumps(scene, indent=2) + "\n")
