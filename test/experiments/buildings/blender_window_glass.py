"""Explicit Eevee glass comparisons; the source material remains the default."""

import math

from thin_glass import sheet_coefficients

PROFILES = (
    "source",
    "clear-principled",
    "screen-clear",
    "screen-slab",
    "thin-slab",
    "transparent-control",
)
IOR = 1.52
ROUGHNESS = 0.02


def thin_sheet(tree, shader):
    """Shade the entering face once; the retained body's exit face is transparent."""
    shader.inputs["Transmission Weight"].default_value = 0.0
    shader.inputs["Metallic"].default_value = 1.0
    shader.inputs["Base Color"].default_value = (1, 1, 1, 1)
    transparent = tree.nodes.new("ShaderNodeBsdfTransparent")
    fresnel = tree.nodes.new("ShaderNodeFresnel")
    fresnel.inputs["IOR"].default_value = IOR
    tree.links.new(
        shader.inputs["Normal"].links[0].from_socket, fresnel.inputs["Normal"]
    )
    twice = tree.nodes.new("ShaderNodeMath")
    twice.operation, twice.inputs[1].default_value = "MULTIPLY", 2
    plus = tree.nodes.new("ShaderNodeMath")
    plus.operation, plus.inputs[1].default_value = "ADD", 1
    divide = tree.nodes.new("ShaderNodeMath")
    divide.operation = "DIVIDE"
    tree.links.new(fresnel.outputs[0], twice.inputs[0])
    tree.links.new(fresnel.outputs[0], plus.inputs[0])
    tree.links.new(twice.outputs[0], divide.inputs[0])
    tree.links.new(plus.outputs[0], divide.inputs[1])
    mixed = tree.nodes.new("ShaderNodeMixShader")
    tree.links.new(divide.outputs[0], mixed.inputs[0])
    tree.links.new(transparent.outputs[0], mixed.inputs[1])
    tree.links.new(shader.outputs[0], mixed.inputs[2])
    geometry = tree.nodes.new("ShaderNodeNewGeometry")
    side = tree.nodes.new("ShaderNodeMixShader")
    tree.links.new(geometry.outputs["Backfacing"], side.inputs[0])
    tree.links.new(mixed.outputs[0], side.inputs[1])
    tree.links.new(transparent.outputs[0], side.inputs[2])
    tree.links.new(side.outputs[0], tree.nodes["Material Output"].inputs["Surface"])


def configure(obj, scene, profile, pane_thickness_m=None):
    if profile not in PROFILES:
        raise ValueError("Unknown explicit glass comparison profile")
    if profile == "screen-slab" and (
        pane_thickness_m is None
        or not math.isfinite(pane_thickness_m)
        or pane_thickness_m <= 0
    ):
        raise ValueError(
            "Screen-slab comparison requires declared positive pane thickness"
        )
    slots = [
        index
        for index, surface in enumerate(obj.data.materials)
        if surface.name == "component-3"
    ]
    if profile != "source" and not slots:
        raise ValueError("Glass comparisons require the fallback physical pane role3")
    triangles = sum(face.material_index in slots for face in obj.data.polygons)
    result = {
        "profile": profile,
        "binding": "Fallback physical role3 only; explicit definitions and facade masks untouched",
        "pane_triangles": triangles,
        "geometry_changed": False,
        "ior": IOR,
        "roughness": 0.12 if profile == "source" else ROUGHNESS,
    }
    if profile == "source":
        return result
    for index in slots:
        surface = obj.data.materials[index]
        tree = surface.node_tree
        shader = tree.nodes["Principled BSDF"]
        shader.inputs["Roughness"].default_value = ROUGHNESS
        if profile in ("screen-clear", "screen-slab"):
            scene.eevee.use_raytracing = True
            scene.eevee.ray_tracing_method = "SCREEN"
            surface.use_raytrace_refraction = True
            if profile == "screen-slab":
                surface.thickness_mode = "SLAB"
                surface.use_thickness_from_shadow = False
                tree.nodes["Material Output"].inputs[
                    "Thickness"
                ].default_value = pane_thickness_m
                result.update(
                    thickness_mode="SLAB",
                    pane_thickness_m=pane_thickness_m,
                    thickness_from_shadow=False,
                    scope="Explicit declared uniform pane thickness, not a guarantee of correct screen-space visibility/refraction",
                )
        elif profile == "thin-slab":
            thin_sheet(tree, shader)
        elif profile == "transparent-control":
            transparent = tree.nodes.new("ShaderNodeBsdfTransparent")
            tree.links.new(
                transparent.outputs[0], tree.nodes["Material Output"].inputs["Surface"]
            )
        if not surface.use_transparent_shadow:
            raise ValueError("Glass comparison requires transparent shadow support")
    if profile == "thin-slab":
        result.update(
            normal_incidence_reflectance=sheet_coefficients(1, IOR)[0],
            optical_model="Average unpolarized interface Fresnel then internal series; first frontface only, exit backface transparent",
            reflection="White metallic GGX lobe at explicit0.02roughness; not an exact smooth delta oracle",
            transmission="Straight, colourless; no absorption, refraction offset, polarization tracking or finite-edge correction",
            sampling="Eevee DITHERED, transparent shadows; no screen raytracing requested",
        )
    return result
