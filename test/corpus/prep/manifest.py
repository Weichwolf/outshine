"""The manifest, read strictly: an unknown key is a refusal, not a shrug."""

import json
import os
import re

from . import licence
from .refusal import Refusal

SCHEMA = "outshine/render-oracle-manifest"
SCHEMA_VERSION = 1

# The runner writes these. The preparer refuses to name them, so a collision has no spelling here.
# Exactly two pictures live in a case directory and the test writes BOTH of them, out of the two
# buffers it computes the score on -- so this preparer produces no image at all. It used to write
# the reference through Blender's own PNG path, which was a second encoding of an image the float
# dump already carried, with a second set of colour-management settings to keep honest and nobody
# checking that the picture and the number agreed. A third image of any kind is what this set exists
# to prevent.
RESERVED_OUTPUT_NAMES = frozenset(["0-reference.png", "1-outshine.png", "outshine.exr",
                                   "outshine.raw", "provenance.json"])

DEFAULT_RECIPE_NAME = "default"
RECIPE_NAME = re.compile(r"^[a-z0-9]([a-z0-9-]*[a-z0-9])?$")

SUBJECT_KINDS = ("gltf", "blend", "generated")
SOURCE_KINDS = ("khronos-sample-assets", "blender-download", "blender-studio", "outshine-generated")
FILE_ROLES = ("gltf", "buffer", "image", "metadata", "blend", "archive-member")
CAMERA_SOURCES = ("manifest", "gltf")
PROJECTIONS = ("perspective", "orthographic")
# "gltf-base-colour" LOWERS THE ORACLE RATHER THAN THE TOLERANCE (doc/requirements.md I.26.13). The
# imported Principled BSDF carries a specular lobe at IOR 1.5 whatever the metallic factor, so a
# textured asset rendered with it has an integral left to perform and no closed form to be judged
# against. This arm keeps what is under test -- the file's own base-colour image, its uv set and its
# sampler -- and replaces the closure with the Diffuse BSDF at roughness 0, whose Cycles form is
# exactly max(dot(N,w),0)/pi. Under a uniform environment that is rho(u,v)*L, per texel, with nothing
# stochastic left.
# "gltf-emissive" IS THE SAME ARM OVER THE OTHER SOCKET. `TextureLinearInterpolationTest` states
# its whole picture in `emissiveFactor`/`emissiveTexture` and carries `[0,0,0,1]` in base colour, so
# the base-colour arm renders its two spheres black and measures nothing. Which socket the asset
# states its picture in is a fact about the asset, so it is declared per case and never guessed.
MATERIAL_SOURCES = ("manifest", "gltf", "gltf-base-colour", "gltf-emissive")
MATERIAL_KINDS = ("diffuse", "emission", "emission-per-material")
# "gltf" IS THE ARM WHERE THE LIGHT CROSSES THE BOUNDARY, and it narrows the refusal rather than
# overturning it (doc/requirements.md I.26.12). `DirectionalLight` and `PointLightIntensityTest`
# state their criteria IN TERMS OF the light in the file -- "the directional lightsource is defined
# as: Intensity = 1.0 lumen / m2, Color = [0.9, 0.8, 0.1]" -- so declaring it beside the asset would
# measure our transcription and not the asset. For every other case the light is declared here and
# whatever crossed is deleted, which is what keeps a rung measuring the light we meant.
LIGHT_KINDS = ("none", "sun", "point", "gltf")
# HOW THE IMPORTER TURNS glTF's PHOTOMETRIC UNITS INTO BLENDER'S RADIOMETRIC ONES, and it is declared
# per case because the two arms are not interchangeable. RAW takes `intensity` across unchanged:
# correct for a directional light, where glTF's lux IS Blender's Sun Strength one to one. For a point
# light RAW makes glTF's candela into Blender's WATTS OF TOTAL POWER, and Cycles then radiates
# `energy / 4pi` per steradian -- a factor of 4pi below the extension's own definition. COMPAT
# multiplies by 4pi and is the arm that makes a point light's candela mean candela.
LIGHTING_MODES = ("RAW", "COMPAT", "SPEC")
# WHETHER THE ORACLE STILL HAS AN ESTIMATOR, and it is a property of the SCENE that has to be
# declared because it decides whether the two-seed acceptance is owed. `delta` is a scene whose only
# source is a light with no area: Cycles samples it in one deterministic direction, there is no
# integral left, and two seeds must agree bit for bit. `selected` is a scene with MORE THAN ONE such
# light: MEASURED at Blender 5.2.0 on `PointLightIntensityTest`, whose eight lights at one sample per
# pixel leave 608 673 of 3 686 400 samples differing between seeds -- Cycles picks ONE light per
# shading event, and which one is the seed's business. There is no setting that samples them all;
# `sample_all_lights_direct` is gone and `use_light_tree` only changes how the one is chosen. So a
# multi-light oracle is a Monte-Carlo estimate and says so here instead of failing a check it cannot
# pass.
LIGHT_ESTIMATORS = ("delta", "selected")
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


def _index(where, value):
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise Refusal(where, expected="a non-negative whole number", observed=repr(value))
    return value


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
            ("schema", "schemaVersion", "id", "title", "covers", "criterion", "subjectClass",
             "acceptanceClass", "subjects", "blender", "scene", "renders"),
            ("notes", "expected", "acceptance", "identicalCoverage", "statedInvariants"),
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
        self.criterion = _criterion(self.document["criterion"])
        self.subjects = _subjects(self.document["subjects"])
        self.blender_version = _blender(self.document["blender"])
        self.scene = _Scene(self.document["scene"])
        self.renders = _renders(self.document["renders"])
        _seed_shift(self.scene.material, self.renders, self.scene.light)
        self.subject_class = _one_of("manifest.subjectClass", self.document["subjectClass"],
                                     ("opaque-min-1px", "sub-pixel-present"))
        # The class the case claims for its placement, and both arms owe an argument: `exact` says
        # what carries the construction, `general-position` says why it cannot. Which one the runner
        # then ENFORCES is the runner's, the same way the thresholds are.
        acceptance_class = _fields("manifest.acceptanceClass", self.document["acceptanceClass"],
                                   ("is", "because"))
        self.acceptance_class = _one_of("manifest.acceptanceClass.is", acceptance_class["is"],
                                        ("exact", "general-position"))
        # THE THRESHOLDS ARE THE RUNNER'S, NOT THIS FILE'S. A manifest's `acceptance` block carries
        # only what it OVERRIDES over the suite's declared defaults, so it is optional and may be
        # absent entirely -- which is what a case that earns no override looks like. What is checked
        # here is the SHAPE every declared number must have; which keys may be overridden, and that an
        # override equal to the default is a refusal, belongs to the one program that reads them.
        self.expected = _quantities("manifest.expected", self.document.get("expected"))
        self.acceptance = _quantities("manifest.acceptance", self.document.get("acceptance"))
        self.identical_coverage = _identical_coverage(
            self.document.get("identicalCoverage"), self.subjects
        )

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
            names.extend(sorted(output_names_for(name).values()))
        return names

    def gltf_names(self):
        """One glTF per subject, and every one of them is imported into the oracle's scene. A
        subject's other files -- the same surface under another index width, the same placement
        through a matrix -- are the RUNNER's to render and score against this one, and none of them
        reaches the oracle."""
        return [s.conversion.settings["outputName"] if s.kind == "blend" else s.entry
                for s in self.subjects]


def output_names_for(recipe_name):
    """The float pair a recipe leaves behind: the EXR the score was defined on and the flat f32 dump
    a C++ reader can be twenty lines long for. No picture -- both pictures are the runner's."""
    suffix = "" if recipe_name == DEFAULT_RECIPE_NAME else "." + recipe_name
    return {"exr": "oracle" + suffix + ".exr", "raw": "oracle" + suffix + ".raw"}


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
        read_file = _generated_file if self.kind == "generated" else _fetched_file
        self.files = [read_file(where + ".files[%d]" % i, f) for i, f in enumerate(field["files"])]
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
        if self.kind != "blend" and self.conversion is not None:
            raise Refusal(where + ".conversion", expected="absent for a " + self.kind + " subject",
                          observed="present")

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
    if kind == "outshine-generated":
        return _fields(where + ".source", value, ("kind",))
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


def _generated_file(where, value):
    """No url, no pin and no licence: there is no upstream to pin against and no third party to
    credit. What stands in the pin's place is that the recipe below is the whole of the input, so
    the preparer computes the digest of what it produced and the oracle key carries that."""
    field = _fields(where, value, ("as", "role", "generator"))
    _name(where, field["as"])
    _one_of(where + ".role", field["role"], FILE_ROLES)
    if not isinstance(field["generator"], dict) or not field["generator"]:
        raise Refusal(where + ".generator", expected="the shape and its parameters", observed=repr(field["generator"]))
    return field


def _fetched_file(where, value):
    field = _fields(where, value, ("url", "sha256", "bytes", "as", "role", "licence"), ("member",))
    if not re.match(r"^[0-9a-f]{64}$", field["sha256"]):
        raise Refusal(where + ".sha256", expected="64 hex digits", observed=field["sha256"])
    _one_of(where + ".role", field["role"], FILE_ROLES)
    _name(where, field["as"])
    if (field["role"] == "archive-member") != ("member" in field):
        raise Refusal(
            where + ".member",
            expected="a member path exactly when the role is archive-member",
            observed=repr(field.get("member")),
        )
    field["licence"] = _licence(where + ".licence", field["licence"])
    return field


def _name(where, value):
    if "/" in value or value.startswith("."):
        raise Refusal(where + ".as", expected="a plain file name in the test directory", observed=value)


def _licence(where, value):
    """Per file and never per repository: a repository-level claim covers nothing in particular."""
    if isinstance(value, list):
        return [_licence(where + "[%d]" % i, v) for i, v in enumerate(value)]
    field = _fields(where, value, ("spdx", "holder", "year", "statedAt"), ("covers",))
    licence.check_spdx(field["spdx"], where)
    return field


# THREE KINDS OF CRITERION, AND THE KIND DECIDES THE INSTRUMENT (doc/requirements.md I.26.12).
# `numeric` -- the asset states a value or a relation, and the acceptance is that number on the
# linear tap. `self-describing` -- the asset renders checkmarks, arrows or markers whose correctness
# is readable FROM THE PICTURE, so the verdict is by eye and the residual against a path tracer's
# own filter is a diagnostic rather than a threshold. `limits-probe` -- the asset states it is not
# expected to render correctly everywhere and has no pass at all.
CRITERION_KINDS = ("numeric", "self-describing", "stated-invariant", "limits-probe")


def _criterion(value):
    """What correct IS, in the asset's own words, with the file those words came from.

    Stated rather than inferred, because the instrument follows from it: a case that quietly moved
    from a number to an eye would be a threshold moving without saying so."""
    # `oracleRole` AND ITS TWO COMPANIONS ARE THE RUNNER'S AND ARE ONLY PASSED THROUGH HERE. They
    # say what Cycles cannot express about this criterion and what that costs, measured; the runner
    # refuses a role with no limitation and a limitation with no measurement, and restating those
    # rules here would put one statement in two places that drift apart. Six manifests carried them
    # while this parser refused the key, which made those six cases unpreparable.
    field = _fields("manifest.criterion", value, ("kind", "says", "statedAt"),
                    ("note", "oracleRole", "oracleLimitation", "oracleLimitationMeasured"))
    _one_of("manifest.criterion.kind", field["kind"], CRITERION_KINDS)
    for key in ("says", "statedAt"):
        if not isinstance(field[key], str) or not field[key]:
            raise Refusal("manifest.criterion." + key, expected="stated", observed=repr(field[key]))
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
        # THE INDEX INTO THE FILE'S OWN `cameras`, AND IT HAS NO DEFAULT. `Cameras` puts a
        # perspective and an orthographic camera on two nodes at one point, so "the camera the file
        # carries" names nothing there; a case that adopted the first would render one of them and
        # report the other's criterion.
        field = _fields("manifest.scene.camera", value, ("source", "index"), ("note",))
        field["index"] = _index("manifest.scene.camera.index", field["index"])
        return field
    # A PARALLEL PROJECTION IS A DIFFERENT MATRIX AND NOT A LONG LENS, so it is declared and never
    # inferred from which field happens to be present. It carries the vertical extent it covers and
    # no field of view at all; a case that needs one needs it for a reason it can state.
    projection = _one_of("manifest.scene.camera.projection", value.get("projection", "perspective"),
                         PROJECTIONS)
    lens = ("yMagM",) if projection == "orthographic" else ("yfovRad", "sensorHeightMm")
    field = _fields(
        "manifest.scene.camera",
        value,
        ("source", "positionM", "lookAtM", "rollRad", "clipStartM", "clipEndM") + lens,
        ("note", "derivation", "projection"),
    )
    field["projection"] = projection
    field["positionM"] = _vector("manifest.scene.camera.positionM", field["positionM"], 3)
    field["lookAtM"] = _vector("manifest.scene.camera.lookAtM", field["lookAtM"], 3)
    for key in ("rollRad", "clipStartM", "clipEndM") + lens:
        field[key] = _number("manifest.scene.camera." + key, field[key])
    return field


def _light(value):
    kind = _one_of("manifest.scene.light.kind", value.get("kind"), LIGHT_KINDS)
    if kind == "none":
        return _fields("manifest.scene.light", value, ("kind",), ("note",))
    if kind == "gltf":
        field = _fields("manifest.scene.light", value, ("kind", "lightingMode", "estimator"),
                        ("note",))
        _one_of("manifest.scene.light.lightingMode", field["lightingMode"], LIGHTING_MODES)
        _one_of("manifest.scene.light.estimator", field["estimator"], LIGHT_ESTIMATORS)
        return field
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
        # THE WHOLE ROW HAS ONE CLOSURE AND IT IS THE FORMAT'S OWN, stated so that a case handing the
        # file its base colour, its metalness and its roughness together cannot be read as handing
        # over only one of the three.
        field = _fields("manifest.scene.material", value, ("source", "kind"), ("note",))
        _one_of("manifest.scene.material.kind", field["kind"], ("metal-rough",))
        return field
    if source in ("gltf-base-colour", "gltf-emissive"):
        # THE CLOSURE IS OURS AND THE COLOUR IS THE FILE'S, and which closure is a declaration
        # because it decides how many integrals are left: `diffuse` is rho*L and holds only where no
        # surface can see another; `emission` removes the world as a light, the sun's disk, a light's
        # radius and visibility at once, and is what a subject that occludes itself must use
        # (doc/requirements.md I.26.13). The colour it emits is still the file's own.
        #
        # AN EMISSIVE COLOUR HAS ONLY THE ONE CLOSURE. glTF's emission is a radiance the surface
        # leaves, not a reflectance an environment is multiplied into, so putting it through a
        # Diffuse BSDF would multiply it by a world radiance the format never asked for.
        field = _fields("manifest.scene.material", value, ("source", "kind"), ("note",))
        closures = ("emission",) if source == "gltf-emissive" else ("diffuse", "emission")
        _one_of("manifest.scene.material.kind", field["kind"], closures)
        return field
    kind = _one_of("manifest.scene.material.kind", value.get("kind"), MATERIAL_KINDS)
    if kind == "diffuse":
        field = _fields("manifest.scene.material", value, ("source", "kind", "colourLinear"), ("note",))
        field["colourLinear"] = _vector("manifest.scene.material.colourLinear", field["colourLinear"], 3)
        return field
    # ONE COLOUR PER NODE, OR ONE PER MATERIAL, AND NO SHORTER SPELLING. Flat emission over touching
    # bodies fuses them into one silhouette, so a misplaced node hides inside the union -- a worse
    # instrument than the ambient-occlusion noise emission was adopted to remove. A map keyed by the
    # file's own name is what makes "the third cube" a fact about the file instead of a position in a
    # list. THE MATERIAL IS THE KEY A MULTI-MATERIAL ASSET HAS: a mesh whose primitives name different
    # materials is one Blender object with several slots, so a per-object colour could reach only one
    # of them.
    keyed = "colourLinearPerNode" if kind == "emission" else "colourLinearPerMaterial"
    field = _fields("manifest.scene.material", value, ("source", "kind", keyed), ("note",))
    where = "manifest.scene.material." + keyed
    if not isinstance(field[keyed], dict) or not field[keyed]:
        raise Refusal(where, expected="a non-empty map of glTF name to linear RGB",
                      observed=repr(field[keyed]))
    field[keyed] = {
        name: _vector(where + "." + name, colour, 3)
        for name, colour in sorted(field[keyed].items())
    }
    return field


SEED_SHIFT_RECIPE_NAME = "seed-shift"


def _seed_shift(material, renders, light):
    """The acceptance an emission case owes, declared rather than argued.

    Cycles does not match itself: a Monte-Carlo estimator's answer depends on its seed. A surface
    that emits its declared colour gathers nothing -- no world sampled as a light, no sun disk, no
    light radius, no visibility -- so two seeds must produce the same bits, and any difference NAMES
    the integral that survived the change. The second recipe therefore differs from the first in the
    seed and in nothing else; anything else would make the comparison a test of that too.
    """
    # WHICH CASES OWE IT: the ones whose render has no estimator left. An emitter gathers nothing.
    # A punctual light with no radius is a delta source, so a scene lit only by such lights and by a
    # world of strength zero is sampled deterministically too -- the same claim, reached from the
    # other side, and the same acceptance.
    reduced = (material.get("kind") in ("emission", "emission-per-material") or
               light.get("estimator") == "delta")
    if not reduced:
        if SEED_SHIFT_RECIPE_NAME in renders:
            raise Refusal("manifest.renders." + SEED_SHIFT_RECIPE_NAME, expected="absent",
                          observed="declared beside a material that is not an emission",
                          why="the seed check is what proves an emitter has no estimator left")
        return
    if SEED_SHIFT_RECIPE_NAME not in renders:
        raise Refusal("manifest.renders", expected="a recipe named " + SEED_SHIFT_RECIPE_NAME,
                      observed=", ".join(sorted(renders)),
                      why="an emission case is accepted on two seeds agreeing bit for bit")
    default, shifted = renders[DEFAULT_RECIPE_NAME], renders[SEED_SHIFT_RECIPE_NAME]
    if default["seed"] == shifted["seed"]:
        raise Refusal("manifest.renders." + SEED_SHIFT_RECIPE_NAME + ".seed",
                      expected="a seed other than " + repr(default["seed"]), observed=repr(shifted["seed"]))
    for key in sorted(default):
        # `note` is prose about the recipe and not a setting Cycles reads.
        if key not in ("seed", "note") and default[key] != shifted[key]:
            raise Refusal("manifest.renders." + SEED_SHIFT_RECIPE_NAME + "." + key,
                          expected=repr(default[key]), observed=repr(shifted[key]),
                          why="the two recipes differ in the seed, so that a difference in the "
                              "output can only be the estimator")


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


def _identical_coverage(value, subjects):
    """Files this case's own render must land in the same pixels as -- decided between two renders
    of ours, with no oracle in it at all, so the agreement is exact rather than within a tolerance.
    Each has to be a file this manifest already declares, so a claim cannot name a subject nothing
    prepares."""
    if value is None:
        return []
    if not isinstance(value, list) or not value:
        raise Refusal("manifest.identicalCoverage", expected="a non-empty list of file names", observed=repr(value))
    declared = set()
    for subject in subjects:
        declared.update(file["as"] for file in subject.files)
    entry = subjects[0].entry
    for name in value:
        if name not in declared:
            raise Refusal("manifest.identicalCoverage", expected="one of " + ", ".join(sorted(declared)),
                          observed=repr(name))
        if name == entry:
            raise Refusal("manifest.identicalCoverage", expected="a file other than the entry " + entry,
                          observed=name, why="a case cannot be evidence of agreeing with itself")
    if len(set(value)) != len(value):
        raise Refusal("manifest.identicalCoverage", expected="distinct names", observed=", ".join(value))
    return list(value)


def _quantities(where, value):
    """Stated before the run and read from there, so a number cannot be edited to match a result."""
    if value is None:
        return {}
    if not isinstance(value, dict):
        raise Refusal(where, expected="an object of declared numbers", observed=repr(value))
    return {name: _quantity(where + "." + name, value[name]) for name in sorted(value)}


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
