"""Shared cached grass meshes for both camera and shadow demand, no blade rebuild."""

import numpy as np
from blender_instances import instance_group
from blender_preview import mesh_object
from organ_preview import leaf_material
from outshine_ref.vegetation import species_site_uniforms


def grass_material(index, count):
    result = leaf_material(f"graminoid variant {index}: procedural rib/transmission")
    # A saved form variant owns its tint; filtered point order is not a colour seed.
    for node in result.node_tree.nodes:
        if node.type == "OBJECT_INFO":
            for link in list(node.outputs["Random"].links):
                target = link.to_socket
                result.node_tree.links.remove(link)
                target.default_value = index / max(1, count - 1)
    return result


def populate_grass(manifest, identity, mesh, sites, selected, masks):
    count = manifest["variants"]
    if manifest.get("format") != "graminoid-tuft-product-1" or not 1 <= count <= 16:
        raise ValueError("explicit bounded cached graminoid mesh product required")
    active = masks["camera"] | masks["sun"]
    if not active.any():
        return 0
    # Original source IDs choose a form independently of camera/query order.
    rows = selected[active]
    variants = (species_site_uniforms(sites["ids"][rows], 9417) * count).astype(int)
    v, t = manifest["vertices_per_variant"], manifest["triangles_per_variant"]
    for index in np.unique(variants):
        first, face_first = int(index) * v, int(index) * t
        faces = mesh["faces"][face_first : face_first + t] - first
        prototype = mesh_object(
            f"grass-{index}",
            mesh["vertices"][first : first + v],
            faces,
            mesh["normals"][face_first * 3 : (face_first + t) * 3],
            grass_material(int(index), count),
            corner_normals=True,
        )
        prototype.data.uv_layers.new(
            name="physical blade coordinates"
        ).data.foreach_set("uv", mesh["uv"][first : first + v][faces.ravel()].ravel())
        members = rows[variants == index]
        for begin in range(0, len(members), 100000):
            part = members[begin : begin + 100000]
            holder = instance_group(
                prototype,
                sites["positions"][part],
                sites["frames"][part],
                sites["scales"][part],
                name=f"grass-{index}-{begin}",
            )
            holder["vegetation_pass_role"] = "all"
            holder["plant_plan_identity"] = identity
    return len(rows) * t
