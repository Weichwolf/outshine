"""Import current building geometry, supplied normals/UVs and metric facade cell attributes."""

import numpy as np
from blender_city_material import base, definition, facade
from outshine_ref.rendering.blender_mesh import mesh_object

DEFAULT_COLOURS = (
    (0.56, 0.48, 0.36),
    (0.68, 0.67, 0.64),
    (0.72, 0.73, 0.74),
    (0.9, 0.96, 1.0),
    (0.34, 0.38, 0.34),
    (0.41, 0.30, 0.18),
    (0.50, 0.41, 0.30),
    (0.09, 0.12, 0.13),
    (0.35, 0.35, 0.35),
    (0.22, 0.24, 0.25),
    (0.16, 0.19, 0.21),
    (0.43, 0.47, 0.49),
    (0.78, 0.76, 0.71),
)

NORMAL_ATTRIBUTE = "building_source_normal"


def source_normals(obj, data, materials):
    expected = data["normals"][data["faces"]].reshape(-1, 3).astype(np.float32)
    attribute = obj.data.attributes.new(NORMAL_ATTRIBUTE, "FLOAT_VECTOR", "CORNER")
    attribute.data.foreach_set("vector", expected.ravel())
    actual = np.empty_like(expected)
    attribute.data.foreach_get("vector", actual.ravel())
    if not np.array_equal(actual, expected):
        raise ValueError("Blender changed the explicit source normal attribute")
    packed = np.empty_like(expected)
    obj.data.corner_normals.foreach_get("vector", packed.ravel())
    for material in materials:
        tree = material.node_tree
        source = tree.nodes.new("ShaderNodeAttribute")
        source.attribute_name = NORMAL_ATTRIBUTE
        transform = tree.nodes.new("ShaderNodeVectorTransform")
        transform.vector_type, transform.convert_from, transform.convert_to = (
            "NORMAL",
            "OBJECT",
            "WORLD",
        )
        normalize = tree.nodes.new("ShaderNodeVectorMath")
        normalize.operation = "NORMALIZE"
        tree.links.new(source.outputs["Vector"], transform.inputs["Vector"])
        tree.links.new(transform.outputs["Vector"], normalize.inputs[0])
        tree.links.new(
            normalize.outputs["Vector"], tree.nodes["Principled BSDF"].inputs["Normal"]
        )
    return {
        "source_normal_attribute": NORMAL_ATTRIBUTE,
        "source_normal_attribute_float32_exact": True,
        "source_normal_attribute_bytes": expected.nbytes,
        "unused_packed_normal_max_error": float(abs(packed - expected).max()),
    }


def cell_sizes(data, identifiers):
    result = np.zeros((len(data["faces"]), 3), dtype=np.float32)
    selected = np.isin(data["materials"], identifiers)
    indices = data["faces"][selected]
    points, uv = data["vertices"][indices], data["uv"][indices]
    a, b = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
    x, y = uv[:, 1] - uv[:, 0], uv[:, 2] - uv[:, 0]
    determinant = x[:, 0] * y[:, 1] - x[:, 1] * y[:, 0]
    if np.any(abs(determinant) < 1e-12):
        raise ValueError("Facade UVs do not define metric cells")
    du = (a * y[:, 1, None] - b * x[:, 1, None]) / determinant[:, None]
    dv = (b * x[:, 0, None] - a * y[:, 0, None]) / determinant[:, None]
    result[selected, 0] = np.linalg.norm(du, axis=1)
    result[selected, 1] = np.linalg.norm(dv, axis=1)
    if not np.isfinite(result).all() or np.any(result[selected, :2] <= 0):
        raise ValueError("Facade cell has invalid metric dimensions")
    return result


def import_mesh(report, data):
    identifiers = np.unique(data["materials"])
    definitions, facades = (
        report["source"]["material_definitions"],
        report["facade_materials"],
    )
    materials = []
    for identifier in identifiers:
        key = str(int(identifier))
        if key in facades:
            wall = definitions[str(facades[key]["wall_material"])]
            surface = facade("facade-" + key, facades[key], wall)
        elif key in definitions:
            surface = definition("material-" + key, definitions[key])
        elif identifier < len(DEFAULT_COLOURS):
            surface = base(
                "component-" + key,
                DEFAULT_COLOURS[identifier],
                0.12 if identifier == 3 else 0.68,
                0.4 if identifier in (2, 7, 10, 11) else 0.0,
            )
            if identifier == 3:
                shader = surface.node_tree.nodes["Principled BSDF"]
                shader.inputs["Transmission Weight"].default_value = 1.0
                shader.inputs["IOR"].default_value = 1.52
        else:
            raise ValueError("Unknown source material: " + key)
        materials.append(surface)
    obj = mesh_object(
        report["city"],
        data["vertices"],
        data["faces"],
        data["normals"],
        materials[0],
        uv=data["uv"],
    )
    for surface in materials[1:]:
        obj.data.materials.append(surface)
    normal_proof = source_normals(obj, data, materials)
    slots = np.searchsorted(identifiers, data["materials"]).astype(np.int32)
    obj.data.polygons.foreach_set("material_index", slots)
    cells = cell_sizes(data, [int(key) for key in facades])
    attribute = obj.data.attributes.new("facade_cell_m", "FLOAT_VECTOR", "CORNER")
    attribute.data.foreach_set("vector", np.repeat(cells, 3, axis=0).ravel())
    if "facade_extent" in data:
        attribute = obj.data.attributes.new(
            "source_facade_extent", "FLOAT_VECTOR", "POINT"
        )
        attribute.data.foreach_set(
            "vector", data["facade_extent"].astype(np.float32).ravel()
        )
    actual = np.empty(data["vertices"].size, dtype=np.float32)
    obj.data.vertices.foreach_get("co", actual)
    actual = actual.reshape(-1, 3)
    if not np.array_equal(actual, data["vertices"].astype(np.float32)):
        raise ValueError("Blender changed supplied positions beyond float32 conversion")
    indices = np.empty(data["faces"].size, dtype=np.int32)
    obj.data.loops.foreach_get("vertex_index", indices)
    if not np.array_equal(indices.reshape(-1, 3), data["faces"]):
        raise ValueError("Blender changed supplied triangle topology")
    return obj, {
        **normal_proof,
        "source_triangles": len(data["faces"]),
        "source_vertices": len(data["vertices"]),
        "float32_position_max_error_m": float(np.max(abs(actual - data["vertices"]))),
        "source_array_bytes": sum(value.nbytes for value in data.values()),
        "normal_domain": "Supplied POINT vectors to explicit CORNER attribute; NORMAL Object/World and Normalize to Principled; packed mesh normals unused for the intended surface normal",
        "uv_domain": "POINT copied to face corners",
        "materials": len(materials),
        "facade_triangles": int(
            np.isin(data["materials"], [int(key) for key in facades]).sum()
        ),
    }
