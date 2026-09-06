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
"""
import json


class Material:
    """One glTF 2.0 material. `to_gltf()` emits the object a glTF file carries verbatim."""

    __slots__ = ("name", "base_color", "metallic", "roughness", "ior", "emissive",
                 "emissive_strength", "alpha_mode", "alpha_cutoff", "double_sided",
                 "transmission")

    def __init__(self, name, base_color, metallic=0.0, roughness=0.85, ior=1.45,
                 emissive=(0.0, 0.0, 0.0), emissive_strength=1.0, alpha_mode="OPAQUE",
                 alpha_cutoff=0.5, double_sided=False, transmission=0.0):
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

    def tinted(self, rgb):
        """The same material in another colour -- an epoch's field over the same render."""
        got = Material(self.name, tuple(rgb) + (self.base_color[3],), self.metallic,
                       self.roughness, self.ior, self.emissive, self.emissive_strength,
                       self.alpha_mode, self.alpha_cutoff, self.double_sided, self.transmission)
        return got

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


add("render",   base_color=(0.55, 0.53, 0.48), roughness=0.92)
add("stucco",   base_color=(0.46, 0.43, 0.36), roughness=0.88)
add("brick",    base_color=(0.20, 0.11, 0.09), roughness=0.90)
add("limestone", base_color=(0.42, 0.40, 0.35), roughness=0.78)
add("sandstone", base_color=(0.40, 0.34, 0.25), roughness=0.82)
add("concrete", base_color=(0.30, 0.30, 0.29), roughness=0.90)
add("asphalt",  base_color=(0.075, 0.075, 0.080), roughness=0.96)
add("paving",   base_color=(0.30, 0.30, 0.28), roughness=0.92)
add("kerbstone", base_color=(0.34, 0.34, 0.32), roughness=0.85)
add("paint",    base_color=(0.62, 0.62, 0.60), roughness=0.70)
add("clay_tile", base_color=(0.21, 0.10, 0.07), roughness=0.85)
add("slate",    base_color=(0.093, 0.097, 0.105), roughness=0.55)
add("copper",   base_color=(0.122, 0.197, 0.168), roughness=0.45, metallic=0.0)
add("zinc",     base_color=(0.24, 0.25, 0.25), roughness=0.30, metallic=1.0)
add("steel",    base_color=(0.55, 0.56, 0.57), roughness=0.35, metallic=1.0)
add("iron",     base_color=(0.12, 0.12, 0.13), roughness=0.55, metallic=1.0)
add("timber",   base_color=(0.18, 0.12, 0.07), roughness=0.75)
add("joinery",  base_color=(0.62, 0.61, 0.58), roughness=0.45)
add("glass",    base_color=(0.040, 0.052, 0.062, 1.0), roughness=0.05, ior=1.5,
    transmission=0.0)
add("grass",    base_color=(0.14, 0.17, 0.09), roughness=0.98)
add("gravel",   base_color=(0.19, 0.18, 0.16), roughness=0.95)
add("water",    base_color=(0.020, 0.035, 0.045), roughness=0.06, ior=1.333)
add("leaf",     base_color=(0.10, 0.16, 0.06), roughness=0.85, alpha_mode="MASK",
    double_sided=True)


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
