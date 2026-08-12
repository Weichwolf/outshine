"""The manifest, read strictly: an unknown key is a refusal, not a shrug."""

import json
import os
import re

from . import licence
from .refusal import Refusal

SCHEMA = "outshine/render-oracle-manifest"
SCHEMA_VERSION = 1

# The runner writes these. The preparer refuses to name them, so a collision has no spelling here.
RESERVED_OUTPUT_NAMES = frozenset(
    ["1-outshine.png", "2-diff.png", "outshine.exr", "outshine.raw", "provenance.json"]
)

DEFAULT_RECIPE_NAME = "default"
RECIPE_NAME = re.compile(r"^[a-z0-9]([a-z0-9-]*[a-z0-9])?$")

SUBJECT_KINDS = ("gltf", "blend")
SOURCE_KINDS = ("khronos-sample-assets", "blender-download", "blender-studio")
FILE_ROLES = ("gltf", "buffer", "image", "metadata", "blend", "archive-member")
CAMERA_SOURCES = ("manifest", "gltf")
MATERIAL_SOURCES = ("manifest", "gltf")
MATERIAL_KINDS = ("diffuse", "emission")
LIGHT_KINDS = ("none", "sun", "point")
WORLD_KINDS = ("factory", "uniform")
DEVICES = ("CPU", "METAL")
FILTER_TYPES = ("BOX", "GAUSSIAN", "BLACKMAN_HARRIS")
ORIGINS = ("SET", "derived", "measured")


def load(path):
    try:
        with open(path, "r") as f:
            document = json.load(f)
    except (OSError, ValueError) as error:
        raise Refusal("manifest " + path, why=str(error))
    return Manifest(document, os.path.dirname(os.path.abspath(path)))


def _fields(where, value, required, optional=()):
    if not isinstance(value, dict):
        raise Refusal(where, expected="an object", observed=type(value).__name__)
    known = set(required) | set(optional)
    for key in sorted(value):
        if key not in known:
            raise Refusal(
                where + "." + key,
                expected="one of " + ", ".join(sorted(known)),
                observed=key,
                why="a key nobody reads is a setting that silently did not apply",
            )
    for key in required:
        if key not in value:
            raise Refusal(where, expected="the field " + repr(key), observed="absent")
    return value


def _one_of(where, value, allowed):
    if value not in allowed:
        raise Refusal(where, expected="one of " + ", ".join(allowed), observed=repr(value))
    return value


def _vector(where, value, length):
    if not isinstance(value, list) or len(value) != length:
        raise Refusal(where, expected=str(length) + " numbers", observed=repr(value))
    for component in value:
        if not isinstance(component, (int, float)) or isinstance(component, bool):
            raise Refusal(where, expected="numbers", observed=repr(value))
    return [float(c) for c in value]


def _number(where, value):
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        raise Refusal(where, expected="a number", observed=repr(value))
    return float(value)


def _boolean(where, value):
    if not isinstance(value, bool):
        raise Refusal(where, expected="true or false", observed=repr(value))
    return value


def _quantity(where, value):
    """Every number carries its origin, so a bare float has no spelling in this file."""
    field = _fields(where, value, ("value", "unit", "origin"), ("derivation", "note"))
    _one_of(where + ".origin", field["origin"], ORIGINS)
    _number(where + ".value", field["value"])
    if not isinstance(field["unit"], str) or not field["unit"]:
        raise Refusal(where + ".unit", expected="a unit, e.g. px or dimensionless", observed=repr(field.get("unit")))
    if field["origin"] == "derived" and not field.get("derivation"):
        raise Refusal(
            where,
            expected="a derivation beside a derived number",
            observed="none",
            why="derived without its derivation is a bare number wearing a label",
        )
    return field


class Manifest:
    def __init__(self, document, directory):
        self.directory = directory
        self.document = _fields(
            "manifest",
            document,
            ("schema", "schemaVersion", "id", "title", "covers", "subjects", "blender", "scene", "renders", "acceptance"),
            ("notes",),
        )
        if self.document["schema"] != SCHEMA:
            raise Refusal("manifest.schema", expected=SCHEMA, observed=self.document["schema"])
        if self.document["schemaVersion"] != SCHEMA_VERSION:
            raise Refusal(
                "manifest.schemaVersion", expected=SCHEMA_VERSION, observed=self.document["schemaVersion"]
            )
        self.id = self.document["id"]
        self.title = self.document["title"]
        self.covers = self.document["covers"]
        self.subjects = _subjects(self.document["subjects"])
        self.blender_version = _blender(self.document["blender"])
        self.scene = _Scene(self.document["scene"])
        self.renders = _renders(self.document["renders"])
        self.acceptance = _acceptance(self.document["acceptance"])

        for name in self.output_names():
            if name in RESERVED_OUTPUT_NAMES:
                raise Refusal(
                    "manifest.renders",
                    expected="an output name outside " + ", ".join(sorted(RESERVED_OUTPUT_NAMES)),
                    observed=name,
                    why="those names belong to the test that reads this directory",
                )
        seen = {}
        for subject in self.subjects:
            for file in subject.files:
                if file["as"] in RESERVED_OUTPUT_NAMES:
                    raise Refusal("manifest.subjects", expected="a name outside the reserved set", observed=file["as"])
                if file["as"] in seen:
                    raise Refusal(
                        "manifest.subjects",
                        expected="one file per name in the test directory",
                        observed="%s is claimed by both %s and %s" % (file["as"], seen[file["as"]], subject.id),
                    )
                seen[file["as"]] = subject.id
            if subject.kind == "blend":
                if subject.conversion.settings["outputName"] in seen:
                    raise Refusal("manifest.subjects", expected="a free name for the conversion output",
                                  observed=subject.conversion.settings["outputName"])
                seen[subject.conversion.settings["outputName"]] = subject.id

    def output_names(self):
        names = []
        for name in sorted(self.renders):
            names.extend(output_names_for(name))
        return names

    def gltf_names(self):
        """One glTF per subject, and every one of them is imported into the oracle's scene."""
        return [s.entry if s.kind == "gltf" else s.conversion.settings["outputName"] for s in self.subjects]


def output_names_for(recipe_name):
    """The eye's file sorts to the top of the directory; the deciding f32 pair sits below it."""
    suffix = "" if recipe_name == DEFAULT_RECIPE_NAME else "." + recipe_name
    return ["oracle" + suffix + ".exr", "0-oracle" + suffix + ".png", "oracle" + suffix + ".raw"]


def _subjects(value):
    if not isinstance(value, list) or not value:
        raise Refusal("manifest.subjects", expected="a non-empty list", observed=repr(value))
    subjects = [_Subject(i, v) for i, v in enumerate(value)]
    identifiers = [s.id for s in subjects]
    if len(set(identifiers)) != len(identifiers):
        raise Refusal("manifest.subjects", expected="distinct ids", observed=", ".join(identifiers))
    return subjects


class _Subject:
    def __init__(self, index, value):
        where = "manifest.subjects[%d]" % index
        field = _fields(
            where,
            value,
            ("id", "kind", "name", "source", "files", "entry"),
            ("attributes", "notes", "conversion"),
        )
        self.id = field["id"]
        self.kind = _one_of(where + ".kind", field["kind"], SUBJECT_KINDS)
        self.name = field["name"]
        licence.check_subject_name(self.name)
        self.source = _source(where, field["source"])
        self.files = [_file(where + ".files[%d]" % i, f) for i, f in enumerate(field["files"])]
        self.entry = field["entry"]
        self.attributes = field.get("attributes", [])
        self.notes = field.get("notes", "")
        self.conversion = _Conversion(where, field["conversion"]) if "conversion" in field else None
        if self.entry not in [f["as"] for f in self.files]:
            raise Refusal(where + ".entry", expected="one of the declared files", observed=self.entry)
        if self.kind == "blend" and self.conversion is None:
            raise Refusal(
                where + ".conversion",
                expected="a conversion block beside a .blend subject",
                observed="absent",
                why="the export settings are the measurement, so none of them is left to a default",
            )
        if self.kind == "gltf" and self.conversion is not None:
            raise Refusal(where + ".conversion", expected="absent for a glTF subject", observed="present")

    def metadata_file(self):
        for file in self.files:
            if file["role"] == "metadata":
                return file
        return None


def _source(where, value):
    field = _fields(
        where + ".source", value, ("kind",), ("commit", "model", "pinnedOn", "pinReason", "page")
    )
    kind = _one_of(where + ".source.kind", field["kind"], SOURCE_KINDS)
    if kind == "khronos-sample-assets":
        for key in ("commit", "model", "pinnedOn", "pinReason"):
            if not field.get(key):
                raise Refusal(where + ".source." + key, expected="stated", observed="absent")
        if not re.match(r"^[0-9a-f]{40}$", field["commit"]):
            raise Refusal(where + ".source.commit", expected="40 hex digits", observed=field["commit"])
    else:
        if not field.get("page"):
            raise Refusal(
                where + ".source.page",
                expected="the page that states this file's licence",
                observed="absent",
                why="a plain URL carries no metadata, so the statement has to be findable",
            )
    return field


def _file(where, value):
    field = _fields(where, value, ("url", "sha256", "bytes", "as", "role", "licence"), ("member",))
    if not re.match(r"^[0-9a-f]{64}$", field["sha256"]):
        raise Refusal(where + ".sha256", expected="64 hex digits", observed=field["sha256"])
    _one_of(where + ".role", field["role"], FILE_ROLES)
    if "/" in field["as"] or field["as"].startswith("."):
        raise Refusal(where + ".as", expected="a plain file name in the test directory", observed=field["as"])
    if (field["role"] == "archive-member") != ("member" in field):
        raise Refusal(
            where + ".member",
            expected="a member path exactly when the role is archive-member",
            observed=repr(field.get("member")),
        )
    field["licence"] = _licence(where + ".licence", field["licence"])
    return field


def _licence(where, value):
    """Per file and never per repository: a repository-level claim covers nothing in particular."""
    if isinstance(value, list):
        return [_licence(where + "[%d]" % i, v) for i, v in enumerate(value)]
    field = _fields(where, value, ("spdx", "holder", "year", "statedAt"), ("covers",))
    licence.check_spdx(field["spdx"], where)
    return field


def _blender(value):
    field = _fields("manifest.blender", value, ("version",), ("note",))
    if not re.match(r"^\d+\.\d+(\.\d+)?$", str(field["version"])):
        raise Refusal("manifest.blender.version", expected="a release version, e.g. 5.2.0", observed=field["version"])
    return str(field["version"])


class _Scene:
    """What is declared beside the glTF because it must never cross it."""

    def __init__(self, value):
        field = _fields("manifest.scene", value, ("frame", "camera", "light", "world", "material"))
        if field["frame"] != "gltf":
            raise Refusal(
                "manifest.scene.frame",
                expected="gltf -- right-handed, +Y up, metres",
                observed=field["frame"],
                why="a vector without its frame is a vector that means whatever the reader assumes",
            )
        self.camera = _camera(field["camera"])
        self.light = _light(field["light"])
        self.world = _world(field["world"])
        self.material = _material(field["material"])

    def as_job(self):
        return {"camera": self.camera, "light": self.light, "world": self.world, "material": self.material}


def _camera(value):
    source = _one_of("manifest.scene.camera.source", value.get("source"), CAMERA_SOURCES)
    if source == "gltf":
        return _fields("manifest.scene.camera", value, ("source",), ("note",))
    field = _fields(
        "manifest.scene.camera",
        value,
        ("source", "positionM", "lookAtM", "rollRad", "yfovRad", "sensorHeightMm", "clipStartM", "clipEndM"),
        ("note", "derivation"),
    )
    field["positionM"] = _vector("manifest.scene.camera.positionM", field["positionM"], 3)
    field["lookAtM"] = _vector("manifest.scene.camera.lookAtM", field["lookAtM"], 3)
    for key in ("rollRad", "yfovRad", "sensorHeightMm", "clipStartM", "clipEndM"):
        field[key] = _number("manifest.scene.camera." + key, field[key])
    return field


def _light(value):
    kind = _one_of("manifest.scene.light.kind", value.get("kind"), LIGHT_KINDS)
    if kind == "none":
        return _fields("manifest.scene.light", value, ("kind",), ("note",))
    if kind == "sun":
        field = _fields(
            "manifest.scene.light",
            value,
            ("kind", "irradianceWPerM2", "directionM", "angleRad", "colourLinear"),
            ("note",),
        )
        field["directionM"] = _vector("manifest.scene.light.directionM", field["directionM"], 3)
        field["colourLinear"] = _vector("manifest.scene.light.colourLinear", field["colourLinear"], 3)
        field["irradianceWPerM2"] = _number("manifest.scene.light.irradianceWPerM2", field["irradianceWPerM2"])
        field["angleRad"] = _number("manifest.scene.light.angleRad", field["angleRad"])
        return field
    field = _fields(
        "manifest.scene.light", value, ("kind", "powerW", "radiusM", "positionM", "colourLinear"), ("note",)
    )
    field["positionM"] = _vector("manifest.scene.light.positionM", field["positionM"], 3)
    field["colourLinear"] = _vector("manifest.scene.light.colourLinear", field["colourLinear"], 3)
    field["powerW"] = _number("manifest.scene.light.powerW", field["powerW"])
    field["radiusM"] = _number("manifest.scene.light.radiusM", field["radiusM"])
    return field


def _world(value):
    kind = _one_of("manifest.scene.world.kind", value.get("kind"), WORLD_KINDS)
    if kind == "factory":
        return _fields("manifest.scene.world", value, ("kind",), ("note",))
    field = _fields("manifest.scene.world", value, ("kind", "colourLinear", "strength"), ("note",))
    field["colourLinear"] = _vector("manifest.scene.world.colourLinear", field["colourLinear"], 3)
    field["strength"] = _number("manifest.scene.world.strength", field["strength"])
    return field


def _material(value):
    source = _one_of("manifest.scene.material.source", value.get("source"), MATERIAL_SOURCES)
    if source == "gltf":
        return _fields("manifest.scene.material", value, ("source",), ("note",))
    field = _fields("manifest.scene.material", value, ("source", "kind", "colourLinear"), ("note",))
    _one_of("manifest.scene.material.kind", field["kind"], MATERIAL_KINDS)
    field["colourLinear"] = _vector("manifest.scene.material.colourLinear", field["colourLinear"], 3)
    return field


def _renders(value):
    if not isinstance(value, dict) or not value:
        raise Refusal("manifest.renders", expected="a non-empty map of recipe name to recipe", observed=repr(value))
    if DEFAULT_RECIPE_NAME not in value:
        raise Refusal(
            "manifest.renders",
            expected="a recipe named " + DEFAULT_RECIPE_NAME,
            observed=", ".join(sorted(value)),
            why="the acceptance numbers are judged on one named recipe, and it is that one",
        )
    out = {}
    for name in sorted(value):
        if not RECIPE_NAME.match(name):
            raise Refusal("manifest.renders." + name, expected="lowercase, digits and hyphens", observed=name)
        out[name] = _recipe("manifest.renders." + name, value[name])
    return out


def _recipe(where, value):
    field = _fields(
        where,
        value,
        (
            "engine",
            "device",
            "resolutionX",
            "resolutionY",
            "samples",
            "adaptiveSampling",
            "denoise",
            "seed",
            "pixelFilter",
            "filmTransparent",
            "filmExposure",
            "scaleLength",
            "bounces",
            "colourManagement",
            "exrCodec",
        ),
        ("note",),
    )
    if field["engine"] != "CYCLES":
        raise Refusal(where + ".engine", expected="CYCLES -- the oracle is the path tracer", observed=field["engine"])
    _one_of(where + ".device", field["device"], DEVICES)
    for key in ("resolutionX", "resolutionY", "samples", "seed"):
        if not isinstance(field[key], int) or isinstance(field[key], bool) or field[key] < 0:
            raise Refusal(where + "." + key, expected="a non-negative integer", observed=repr(field[key]))
    for key in ("adaptiveSampling", "denoise", "filmTransparent"):
        _boolean(where + "." + key, field[key])
    if field["adaptiveSampling"]:
        raise Refusal(
            where + ".adaptiveSampling",
            expected="false",
            observed="true",
            why="a sample count with an adaptive threshold under it is not a stated sample count",
        )
    if field["denoise"]:
        raise Refusal(
            where + ".denoise", expected="false", observed="true", why="a denoiser is an estimator with no error bar"
        )
    if not field["filmTransparent"]:
        raise Refusal(
            where + ".filmTransparent",
            expected="true",
            observed="false",
            why="alpha is the exact coverage channel and it costs nothing",
        )
    for key in ("filmExposure", "scaleLength"):
        field[key] = _number(where + "." + key, field[key])
    field["pixelFilter"] = _pixel_filter(where + ".pixelFilter", field["pixelFilter"])
    field["bounces"] = _bounces(where + ".bounces", field["bounces"])
    field["colourManagement"] = _colour_management(where + ".colourManagement", field["colourManagement"])
    _one_of(where + ".exrCodec", field["exrCodec"], ("NONE", "ZIP", "PIZ"))
    return field


def _pixel_filter(where, value):
    field = _fields(where, value, ("type", "widthPx"), ("note",))
    _one_of(where + ".type", field["type"], FILTER_TYPES)
    field["widthPx"] = _number(where + ".widthPx", field["widthPx"])
    if field["widthPx"] < 0.01:
        raise Refusal(where + ".widthPx", expected=">= 0.01, the RNA minimum", observed=field["widthPx"])
    return field


def _bounces(where, value):
    field = _fields(where, value, ("max", "diffuse", "glossy", "transmission", "volume", "transparentMax"))
    for key in field:
        if not isinstance(field[key], int) or isinstance(field[key], bool) or field[key] < 0:
            raise Refusal(where + "." + key, expected="a non-negative integer", observed=repr(field[key]))
    return field


def _colour_management(where, value):
    field = _fields(where, value, ("displayDevice", "viewTransform", "look", "exposure", "gamma"), ("note",))
    for key in ("exposure", "gamma"):
        field[key] = _number(where + "." + key, field[key])
    return field


def _acceptance(value):
    """Stated before the run and read from here, so a number cannot be edited to match a result."""
    if not isinstance(value, dict) or not value:
        raise Refusal("manifest.acceptance", expected="a non-empty object", observed=repr(value))
    out = {}
    for group in sorted(value):
        entries = value[group]
        if not isinstance(entries, dict) or not entries:
            raise Refusal("manifest.acceptance." + group, expected="a non-empty object", observed=repr(entries))
        out[group] = {}
        for name in sorted(entries):
            out[group][name] = _quantity("manifest.acceptance.%s.%s" % (group, name), entries[name])
    return out


class _Conversion:
    def __init__(self, where, value):
        self.settings = _fields(
            where + ".conversion", value, ("exportSettings", "frameStart", "outputName"), ("note",)
        )
        if not isinstance(self.settings["exportSettings"], dict) or not self.settings["exportSettings"]:
            raise Refusal(
                where + ".conversion.exportSettings",
                expected="every export setting written out",
                observed=repr(self.settings["exportSettings"]),
                why="three of the exporter's defaults are wrong for us and one is exclusive with another",
            )
        if self.settings["outputName"] in RESERVED_OUTPUT_NAMES:
            raise Refusal(
                where + ".conversion.outputName",
                expected="a name outside the reserved set",
                observed=self.settings["outputName"],
            )
        # One file per conversion, so the whole product has one hash and one name in the store.
        # GLTF_SEPARATE writes a .bin and a texture directory beside the .gltf, and a store keyed on
        # the .gltf alone loses them -- measured: the import then fails on a missing scene.bin.
        if self.settings["exportSettings"].get("export_format") != "GLB":
            raise Refusal(
                where + ".conversion.exportSettings.export_format",
                expected="GLB",
                observed=repr(self.settings["exportSettings"].get("export_format")),
                why="a multi-file export cannot be one entry in a content store keyed by hash = filename",
            )
