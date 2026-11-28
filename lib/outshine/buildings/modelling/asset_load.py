"""Convert decoded scene geometry to a shared prepared asset with explicit source provenance."""

from dataclasses import dataclass, replace
import hashlib
from pathlib import Path

import numpy as np

from .asset_decode import decode
from .faces import Face
from .insert import Asset
from .metadata import Material
from .shape import Shape


@dataclass(frozen=True)
class ImportInfo:
    path: str
    sha256: str
    dependencies: tuple
    nodes: tuple
    omitted: tuple
    uv_convention: str
    ignored: tuple = ("tangents", "custom attributes")


def transformed(mesh, matrix):
    if not np.isfinite(matrix).all():
        raise ValueError("Asset scene transform is non-finite")
    basis = np.asarray(matrix[:3, :3], dtype=float)
    determinant = float(np.linalg.det(basis))
    if determinant == 0:
        raise ValueError("Asset scene transform is singular")
    vertices = np.asarray(mesh.vertices) @ basis.T + matrix[:3, 3]
    normals = np.asarray(mesh.vertex_normals) @ np.linalg.inv(basis)
    lengths = np.linalg.norm(normals, axis=1)
    if np.any(lengths == 0):
        raise ValueError("Asset contains invalid vertex normals")
    normals /= lengths[:, None]
    triangles = np.asarray(mesh.faces, dtype=np.int32)
    if determinant < 0:
        triangles = triangles[:, ::-1]
    return vertices, normals, triangles


def channels(mesh):
    result = {}
    if getattr(mesh.visual, "uv", None) is not None:
        result["uv"] = np.asarray(mesh.visual.uv, dtype=float)
    for key, value in result.items():
        if len(value) != len(mesh.vertices) or not np.isfinite(value).all():
            raise ValueError("Invalid imported asset channel: " + key)
    return result


def load_asset(path, name="", up_axis="yUp"):
    path = Path(path).resolve()
    world, data, material_ids, definitions, omitted, dependencies, resources = decode(
        path
    )
    if len(definitions) > 65534:
        raise ValueError("Asset exceeds native material identifiers")
    material_defs = {index + 1: value for index, value in enumerate(definitions)}
    material_defs[0] = Material("unassigned", {})
    vertices = []
    normals = []
    triangles = []
    faces = []
    groups = {}
    nodes = []
    channel_rows = []
    offset = 0
    for group, node in enumerate(sorted(world.graph.nodes_geometry)):
        matrix, geometry_name = world.graph[node]
        mesh = world.geometry[geometry_name]
        points, directions, indices = transformed(mesh, matrix)
        extra = channels(mesh)
        material = material_ids[geometry_name]
        material = 0 if material is None else material + 1
        if material not in material_defs:
            raise ValueError("Asset references an unknown material")
        groups[group] = str(node)
        uv = extra.get("uv")
        for row in indices:
            corner = () if uv is None else (tuple(tuple(pair) for pair in uv[row]),)
            channels_uv = tuple(
                (int(key[2:]), (tuple(tuple(pair) for pair in value[row]),))
                for key, value in extra.items()
                if key.startswith("uv") and key != "uv"
            )
            faces.append(
                Face(
                    tuple(int(i) + offset for i in row),
                    material=material,
                    group=group,
                    tags=("asset.imported",),
                    uvs=corner,
                    uv_sets=channels_uv,
                )
            )
        triangles.append(indices + offset)
        vertices.append(points)
        normals.append(directions)
        channel_rows.append(extra)
        offset += len(points)
        authored = "vertex_normals" in data["geometry"][geometry_name]
        nodes.append(
            (
                str(node),
                str(geometry_name),
                tuple(tuple(float(x) for x in row) for row in matrix),
                "provided" if authored else "library-derived",
                tuple(sorted(extra)),
            )
        )
    if not vertices:
        raise ValueError("Asset has no visible surface nodes")
    mesh = dict(
        vertices=np.concatenate(vertices),
        faces=np.concatenate(triangles),
        normals=np.concatenate(normals),
        parts=np.arange(len(faces), dtype=np.int32),
        materials=np.array([face.material for face in faces], dtype=np.uint16),
        groups=np.array([face.group for face in faces], dtype=np.uint32),
    )
    keys = set().union(*(row.keys() for row in channel_rows))
    for key in sorted(keys):
        width = next(row[key].shape[1] for row in channel_rows if key in row)
        mesh[key] = np.concatenate(
            [
                row.get(key, np.zeros((len(points), width)))
                for points, row in zip(vertices, channel_rows, strict=True)
            ]
        )
    source = Shape.from_mesh(
        mesh, polygons=faces, group_names=groups, material_defs=material_defs
    )
    asset = (
        Asset(source.geometry, source.scope.position, name or path.stem)
        if up_axis == "yUp"
        else Asset.from_shape(source, name or path.stem, up_axis)
    )
    info = ImportInfo(
        str(path),
        hashlib.sha256(path.read_bytes()).hexdigest(),
        tuple(sorted(dependencies.items())),
        tuple(nodes),
        omitted,
        "OBJ lower-left",
    )
    return replace(asset, import_info=info, resources=resources)
