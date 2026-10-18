"""Local batched Blender binding of cached window planes and physical wood."""

import bpy
import numpy as np
from cloud_product import pack_atlas
from conifer_whole_preview import conifer_material
from PIL import Image


def textures(bank, directory):
    directory.mkdir()
    images, uvs = [], []
    leaf = np.zeros((320, 320), dtype=np.uint8)
    for index, (image, (frame, *_)) in enumerate(bank):
        normal = image[..., 4:7] @ frame
        rgba = np.concatenate(
            ((normal + image[..., 3, None]) / 2, image[..., 3, None]), axis=-1
        )
        images.append(
            tuple(
                np.uint8(np.rint(np.clip(a, 0, 1) * 255))
                for a in (image[..., :4], rgba)
            )
        )
        uvs.append(np.zeros((4, 2)))
        x, y = index % 8 * 40 + 4, index // 8 * 40 + 4
        leaf[y : y + 32, x : x + 32] = np.uint8(
            np.rint(np.clip(image[..., 7], 0, 1) * 255)
        )
    pack_atlas(directory, images, uvs, 32)
    Image.fromarray(np.flipud(leaf)).save(directory / "leaf.png")
    return {
        "colour": "cloud-colour.png",
        "normal": "cloud-normal.png",
        "leaf_weight": "leaf.png",
    }


def bind_mesh(name, data, surface):
    vertices, faces = data["vertices"], data["faces"]
    if not 0 < len(faces) <= 3000000 or not np.isfinite(vertices).all():
        raise ValueError(
            "bounded finite existing geometry required before Blender binding"
        )
    if faces.min() < 0 or faces.max() >= len(vertices):
        raise ValueError("valid existing triangle indices required")
    mesh = bpy.data.meshes.new(name)
    mesh.vertices.add(len(vertices))
    mesh.vertices.foreach_set("co", np.asarray(vertices, np.float32).ravel())
    mesh.loops.add(faces.size)
    mesh.loops.foreach_set("vertex_index", np.asarray(faces, np.int32).ravel())
    mesh.polygons.add(len(faces))
    mesh.polygons.foreach_set("loop_start", np.arange(len(faces), dtype=np.int32) * 3)
    mesh.polygons.foreach_set("loop_total", np.full(len(faces), 3, dtype=np.int32))
    mesh.materials.append(surface)
    mesh.update()
    if "uv" in data:
        mesh.uv_layers.new(name="physical source UV").data.foreach_set(
            "uv", np.asarray(data["uv"][faces], np.float32).ravel()
        )
        mesh.attributes.new("window_tail_weight", "FLOAT", "FACE").data.foreach_set(
            "value", np.asarray(data["weights"], np.float32)
        )
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def weighted_material(name, directory, maps, role):
    result = conifer_material(name, directory, maps, role)
    nodes, links = result.node_tree.nodes, result.node_tree.links
    factors = [
        n.inputs[0]
        for n in nodes
        if n.type == "MIX_SHADER"
        and n.inputs[0].is_linked
        and n.inputs[0].links[0].from_socket.name == "Alpha"
    ]
    if len(factors) != 1:
        raise ValueError("one existing explicit source-alpha coverage factor required")
    original = factors[0].links[0].from_socket
    attribute = nodes.new("ShaderNodeAttribute")
    attribute.attribute_name = "window_tail_weight"
    factor = nodes.new("ShaderNodeMath")
    factor.operation = "MULTIPLY"
    links.new(original, factor.inputs[0])
    links.new(attribute.outputs["Fac"], factor.inputs[1])
    links.new(factor.outputs[0], factors[0])
    return result
