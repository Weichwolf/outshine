"""MATERIALS IN glTF 2.0's OWN TERMS, because glTF is the one format this tree imports.

CLAUDE.md: "glTF is ONE format this tree ships an importer for, and apart from that importer
nothing in the engine, the door or a generator knows or allows for it". The corollary is that a
generator's material vocabulary should BE glTF's, so the handover is a copy and not a translation
-- and glTF 2.0's model is `pbrMetallicRoughness`, which is also exactly what Blender's Principled
BSDF and every modern engine implement.

    baseColorFactor      RGBA, LINEAR, and it is an ALBEDO -- the fraction of light returned.
                         Not a colour picked off a screen: a wall at 0.86 does not exist
    metallicFactor       0 for every dielectric (render, brick, stone, glass, paint, wood),
                         1 for a metal. There is nothing in between except a mix of the two
    roughnessFactor      0 mirror, 1 fully diffuse. Polished stone 0.3, render 0.9, asphalt 0.95
    emissiveFactor       + KHR_materials_emissive_strength: a lit window, a lamp, a sign
    alphaMode            OPAQUE, MASK (a leaf, a chain-link fence) or BLEND
    KHR_materials_ior    1.5 for glass, 1.45 for most dielectrics
    KHR_materials_transmission   real glass, where a scenario asks for it

MEASURED ALBEDOS, which is where the numbers come from:

    fresh white render  0.70..0.80   aged render        0.50..0.60   cream stucco   0.40..0.55
    red brick           0.15..0.25   limestone          0.35..0.50   concrete       0.25..0.35
    clay roof tile      0.15..0.25   slate              0.08..0.12   copper (green) 0.18..0.22
    asphalt             0.07..0.12   concrete paving    0.25..0.35   grass          0.18..0.25
    window glass        ~0.05 diffuse; the rest is specular and transmission

A material with the wrong RATIO to its neighbour cannot be fixed by exposure, because exposure
moves the whole picture: the ratio IS what a viewer reads a material from.

AND THREE NUMBERS glTF's CORE DOES NOT CARRY, because glTF expects a TEXTURE where this engine
wants a PARAMETER. CLAUDE.md lists "parameterised materials" among the five things the frame
budget is laid out for, and names the quantity a generator owes: RELIEF DEPTH per facade area.
A texture is somebody else's photograph of a surface; these three are the surface's own dimensions
and they go in glTF's `extras`, which is exactly what `extras` is for:

    grain_m     the size of the surface's own repeating feature, in METRES. Asphalt's is its
                aggregate (AC 11 D S: 11 mm nominal); a render's is its float mark
    relief_m    how deep that feature stands, in METRES. It is what makes raking light read a
                surface at all, and it is the number a normal map would otherwise hide
    mottle      how much the albedo varies over the grain, as a FRACTION of the albedo. Zero is
                a plastic; a real brick wall runs 0.15 to 0.30 brick to brick
    unit_m      A SURFACE THAT IS LAID IS NOT NOISE. Brick, ashlar, paving, tile and slate are
                UNITS in a bond, and noise at their size reads as dirt rather than as masonry
                (rendered a wall at 84 mm grain and looked at, 2026-09-06). So a laid material
                carries the unit it is laid in -- (length, height) in metres -- and
    joint_m     the joint between two of them, with `relief_m` as how deep it is raked, and
    bond        how the next course is offset: `stretcher` is half a unit (Laeuferverband and
                every running bond there is), `stack` is none, `` is not laid at all

The pattern is projected TRIPLANAR from world metres, so a course runs horizontally on any wall
and a paving joint lies in the ground plane, with no UV anywhere in the generator. That is the
same box mapping a fragment stage does, which is where this belongs in the engine.

They are DIMENSIONS and not decoration: a viewer standing 2 m from a wall resolves 0.5 mm, so a
0.5 mm relief on a 11 mm grain is the difference between asphalt and grey plastic, and the lab
renders both to prove it.
"""
import json


class Material:
    """One glTF 2.0 material. `to_gltf()` emits the object a glTF file carries verbatim."""

    __slots__ = ("name", "base_color", "metallic", "roughness", "ior", "emissive",
                 "emissive_strength", "alpha_mode", "alpha_cutoff", "double_sided",
                 "transmission", "grain_m", "relief_m", "mottle", "unit_m", "joint_m",
                 "bond")

    def __init__(self, name, base_color, metallic=0.0, roughness=0.85, ior=1.45,
                 emissive=(0.0, 0.0, 0.0), emissive_strength=1.0, alpha_mode="OPAQUE",
                 alpha_cutoff=0.5, double_sided=False, transmission=0.0,
                 grain_m=0.0, relief_m=0.0, mottle=0.0, unit_m=(0.0, 0.0),
                 joint_m=0.0, bond=""):
        self.name = name
        self.base_color = tuple(base_color) if len(base_color) == 4 else tuple(base_color) + (1.0,)
        self.metallic = float(metallic)
        self.roughness = float(roughness)
        self.ior = float(ior)
        self.emissive = tuple(emissive)
        self.emissive_strength = float(emissive_strength)
        self.alpha_mode = alpha_mode
        self.alpha_cutoff = float(alpha_cutoff)
        self.double_sided = bool(double_sided)
        self.transmission = float(transmission)
        self.grain_m = float(grain_m)
        self.relief_m = float(relief_m)
        self.mottle = float(mottle)
        self.unit_m = (float(unit_m[0]), float(unit_m[1]))
        self.joint_m = float(joint_m)
        self.bond = str(bond)

    def tinted(self, rgb):
        """The same material in another colour -- an epoch's field over the same render."""
        return Material(self.name, tuple(rgb) + (self.base_color[3],), self.metallic,
                        self.roughness, self.ior, self.emissive, self.emissive_strength,
                        self.alpha_mode, self.alpha_cutoff, self.double_sided, self.transmission,
                        self.grain_m, self.relief_m, self.mottle, self.unit_m,
                        self.joint_m, self.bond)

    def to_gltf(self):
        out = {
            "name": self.name,
            "pbrMetallicRoughness": {
                "baseColorFactor": list(self.base_color),
                "metallicFactor": self.metallic,
                "roughnessFactor": self.roughness,
            },
            "alphaMode": self.alpha_mode,
            "doubleSided": self.double_sided,
        }
        if self.alpha_mode == "MASK":
            out["alphaCutoff"] = self.alpha_cutoff
        if self.grain_m > 0.0 or self.bond:
            out["extras"] = {"grain_m": self.grain_m, "relief_m": self.relief_m,
                             "mottle": self.mottle, "unit_m": list(self.unit_m),
                             "joint_m": self.joint_m, "bond": self.bond}
        if any(v > 0.0 for v in self.emissive):
            out["emissiveFactor"] = list(self.emissive)
        ext = {}
        if abs(self.ior - 1.5) > 1e-6:
            ext["KHR_materials_ior"] = {"ior": self.ior}
        if self.emissive_strength != 1.0:
            ext["KHR_materials_emissive_strength"] = {"emissiveStrength": self.emissive_strength}
        if self.transmission > 0.0:
            ext["KHR_materials_transmission"] = {"transmissionFactor": self.transmission}
        if ext:
            out["extensions"] = ext
        return out

    def __repr__(self):
        return f"Material({self.name}, albedo={tuple(round(v, 3) for v in self.base_color[:3])})"


# THE STOCK, by what a thing is MADE OF and not by where it sits. A role in a generator maps to
# one of these; a palette then tints the base colour to the epoch's own field.
STOCK = {}


def add(name, **kw):
    STOCK[name] = Material(name, **kw)
    return STOCK[name]


add("render",   base_color=(0.55, 0.53, 0.48), roughness=0.92, grain_m=0.003, relief_m=0.0004, mottle=0.1)
add("stucco",   base_color=(0.46, 0.43, 0.36), roughness=0.88, grain_m=0.006, relief_m=0.0008, mottle=0.14)
add("brick",    base_color=(0.20, 0.11, 0.09), roughness=0.90, grain_m=0.084, relief_m=0.008, mottle=0.22, unit_m=(0.24, 0.0715), joint_m=0.0125, bond='stretcher')
add("limestone", base_color=(0.42, 0.40, 0.35), roughness=0.78, grain_m=0.25, relief_m=0.004, mottle=0.12, unit_m=(0.6, 0.3), joint_m=0.008, bond='stretcher')
add("sandstone", base_color=(0.40, 0.34, 0.25), roughness=0.82, grain_m=0.25, relief_m=0.005, mottle=0.16, unit_m=(0.6, 0.3), joint_m=0.008, bond='stretcher')
add("concrete", base_color=(0.30, 0.30, 0.29), roughness=0.90, grain_m=0.008, relief_m=0.0005, mottle=0.06)
add("asphalt",  base_color=(0.075, 0.075, 0.080), roughness=0.96, grain_m=0.011, relief_m=0.0015, mottle=0.05)
add("paving",   base_color=(0.30, 0.30, 0.28), roughness=0.92, grain_m=0.2, relief_m=0.005, mottle=0.1, unit_m=(0.3, 0.3), joint_m=0.004, bond='stretcher')
add("kerbstone", base_color=(0.34, 0.34, 0.32), roughness=0.85, grain_m=0.1, relief_m=0.004, mottle=0.08, unit_m=(1.0, 0.3), joint_m=0.005, bond='stack')
add("paint",    base_color=(0.62, 0.62, 0.60), roughness=0.70, grain_m=0.011, relief_m=0.0004, mottle=0.02)
add("clay_tile", base_color=(0.21, 0.10, 0.07), roughness=0.85, grain_m=0.22, relief_m=0.014, mottle=0.18, unit_m=(0.33, 0.3), joint_m=0.006, bond='stretcher')
add("slate",    base_color=(0.093, 0.097, 0.105), roughness=0.55, grain_m=0.18, relief_m=0.004, mottle=0.14, unit_m=(0.3, 0.1), joint_m=0.003, bond='stretcher')
add("copper",   base_color=(0.122, 0.197, 0.168), roughness=0.45, metallic=0.0, grain_m=0.5, relief_m=0.006, mottle=0.2, unit_m=(3.0, 0.53), joint_m=0.004, bond='stack')
add("zinc",     base_color=(0.24, 0.25, 0.25), roughness=0.30, metallic=1.0, grain_m=0.5, relief_m=0.005, mottle=0.08, unit_m=(3.0, 0.43), joint_m=0.004, bond='stack')
add("steel",    base_color=(0.55, 0.56, 0.57), roughness=0.35, metallic=1.0)
add("iron",     base_color=(0.12, 0.12, 0.13), roughness=0.55, metallic=1.0, grain_m=0.002, relief_m=0.0001, mottle=0.04)
add("timber",   base_color=(0.18, 0.12, 0.07), roughness=0.75, grain_m=0.015, relief_m=0.003, mottle=0.2, unit_m=(3.0, 0.15), joint_m=0.003, bond='stack')
add("joinery",  base_color=(0.62, 0.61, 0.58), roughness=0.45, grain_m=0.0, relief_m=0.0, mottle=0.02)
add("glass",    base_color=(0.040, 0.052, 0.062, 1.0), roughness=0.05, ior=1.5,
    transmission=0.0)
add("grass",    base_color=(0.14, 0.17, 0.09), roughness=0.98, grain_m=0.04, relief_m=0.006, mottle=0.22)
add("gravel",   base_color=(0.19, 0.18, 0.16), roughness=0.95, grain_m=0.025, relief_m=0.005, mottle=0.16)
add("water",    base_color=(0.020, 0.035, 0.045), roughness=0.06, ior=1.333)
add("leaf",     base_color=(0.10, 0.16, 0.06), roughness=0.85, alpha_mode="MASK",
    double_sided=True, grain_m=0.0, relief_m=0.0, mottle=0.1)
add("bark",     base_color=(0.085, 0.070, 0.055), roughness=0.94, grain_m=0.03, relief_m=0.006, mottle=0.22)
add("masonry",  base_color=(0.28, 0.26, 0.22), roughness=0.92, grain_m=0.2, relief_m=0.012, mottle=0.2, unit_m=(0.4, 0.22), joint_m=0.02, bond='stretcher')


def gltf_materials(names):
    """The glTF `materials` array for a set of names, and the index each name landed at."""
    order = list(names)
    return [STOCK[n].to_gltf() for n in order], {n: k for k, n in enumerate(order)}


def dump(path, names=None):
    """The stock as a glTF fragment, so what the lab declares can be diffed against what the
    engine imports."""
    got, _ = gltf_materials(names or sorted(STOCK))
    pathlib_write(path, json.dumps({"materials": got}, indent=2))
    return path


def pathlib_write(path, text):
    import pathlib
    pathlib.Path(path).write_text(text)
