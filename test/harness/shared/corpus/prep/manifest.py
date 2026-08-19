"""The manifest, read strictly: an unknown key is a refusal, not a shrug.

WHAT A MANIFEST MAY CONTAIN IS NOT HERE. It is `test/harness/shared/corpus/manifest-schema.json`, which the render
runner reads too, so a key exists once. What is here is the POLICY this preparer owns: which Cycles
settings make an oracle render meaningless, which acceptance a reduced scene owes, and which names
belong to the test that reads the case directory.
"""

import json
import sys
import os
import re

from . import licence, schema
from .refusal import Refusal

SCHEMA = schema.SCHEMA
SCHEMA_VERSION = schema.SCHEMA_VERSION

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

# THE QUANTITIES THE ORACLE IS ASKED FOR BESIDES THE PICTURE. Cycles has answered one question until
# now -- is this IMAGE right -- and a whole-image tail cannot say WHICH term of a shading model is
# wrong. It has render passes, so it can answer `is this QUANTITY right`, and each row turns a band
# of the tail into an attribution. A third quantity is a ROW HERE and not another round, which is the
# whole reason this is a table.
#
# Every one lands in the same `OSRAWF32` layout as the picture, through the same reader, so no
# quantity gets a format of its own.
#
# THE SET IS WHAT A TEST READS TODAY, and the rule is a measured one rather than a preference: an
# invalidation costs ~5 minutes and a channel costs ~15 MB per case per recipe, so ADDING a channel
# is cheap and HOLDING one is not. Eighteen channels measured 293 MB a case and 19.9 GB across the
# corpus, which is a constraint on a 50 GB disk; three is a rounding error. A channel arrives when a
# test reads it.
#
# DROPPED WITH THEIR REASONS, so the next round does not rediscover them:
#   depth      -- the router's WEAKER implementation; index is exact and no test reads depth today.
#   position, uv, ambientOcclusion         -- no bound derivable from named terms, nothing reads them.
#   diffuse/glossy/transmission {Color, Direct, Indirect}, emission, environment -- these attribute
#                 the six unattributed cases and are the first expected back. NEXT round's
#                 justification, not this round's speculation.
#   roughness  -- Cycles publishes NO roughness pass; measured on this host, there is no
#                 `use_pass_roughness` among the view layer's flags. It would need a SHADER AOV wired
#                 into every material, where a material the wiring missed reports zero and zero reads
#                 as an answer -- the same defect `objectIndex` was caught with. Needs a
#                 wiring-completeness check first, and that is its own item.
QUANTITY_PASSES = {
    # WHAT THE SHADING DISAGREEMENT IS ABOUT: a highlight measured 4.2-10.3 degrees out of place, and
    # inferring a normal from where a highlight landed is not a measurement of a normal.
    "normal": {"socket": "Normal", "viewLayerFlag": "use_pass_normal"},
    # THE ROUTER'S PREDICATE. The picture bound asks `is this pixel covered` when the question is
    # `WHAT covers it`, and a surface swap read as 209 codes for want of this.
    "materialIndex": {"socket": "Material Index", "viewLayerFlag": "use_pass_material_index"},
    "objectIndex": {"socket": "Object Index", "viewLayerFlag": "use_pass_object_index"},
    # WHERE THE SURFACE WAS SAMPLED (board:1126). The shading disagreement is now known to be a
    # magnitude and not an orientation -- the tangential direction agrees to a fiftieth of a degree
    # while the tilt does not -- and the leading candidate is that we point-sample a 2048-square normal
    # map under heavy minification while Cycles filters over the ray footprint. Testing that needs the
    # uv the tap was taken at, and with it the footprint-averaged tap is computable on the CPU from the
    # texture already on disk: no attachment, no shader change.
    "uv": {"socket": "UV", "viewLayerFlag": "use_pass_uv"},
}
SEED_SHIFT_RECIPE_NAME = "seed-shift"
RECIPE_NAME = re.compile(r"^[a-z0-9]([a-z0-9-]*[a-z0-9])?$")


def load(path):
    try:
        with open(path, "r") as f:
            document = json.load(f)
    except (OSError, ValueError) as error:
        raise Refusal("manifest " + path, why=str(error))
    return Manifest(document, os.path.dirname(os.path.abspath(path)))


class Manifest:
    def __init__(self, document, directory):
        self.directory = directory
        self.document = schema.check("manifest", "manifest", document)
        self.id = self.document["id"]
        self.title = self.document["title"]
        self.covers = self.document["covers"]
        self.criterion = self.document["criterion"]
        self.subjects = _subjects(self.document["subjects"])
        # A UI CASE HAS NO ORACLE AND SO NO ORACLE POLICY. Everything below this line is about asking
        # Blender a question, and a document that states its own layout asks nobody: it is fetched
        # like any other subject and decided by the test that reads it.
        self.oracle = self.document["schema"] == "outshine/render-oracle-manifest"
        if not self.oracle:
            self.viewport = self.document["viewport"]
            self.blender_version = None
            self.scene = None
            self.renders = {}
            self.subject_class = None
            self.acceptance_class = None
            self.expected = {}
            self.acceptance = {}
            self.identical_coverage = ()
            _check_names(self.subjects)
            return
        self.blender_version = str(self.document["blender"]["version"])
        self.scene = _Scene(self.document["scene"])
        self.renders = _renders(self.document["renders"])
        _seed_shift(self.scene.material, self.renders, self.scene.light)
        self.subject_class = self.document["subjectClass"]
        # The class the case claims for its placement, and both arms owe an argument: `exact` says
        # what carries the construction, `general-position` says why it cannot. Which one the runner
        # then ENFORCES is the runner's, the same way the thresholds are.
        self.acceptance_class = self.document["acceptanceClass"]["is"]
        # THE THRESHOLDS ARE THE RUNNER'S, NOT THIS FILE'S. Stated before the run and read from
        # there, so a number cannot be edited to match a result.
        self.expected = self.document.get("expected", {})
        self.acceptance = self.document.get("acceptance", {})
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
        _check_names(self.subjects)


    def frame_grid(self):
        """Which frames this case is judged at, in order. `[None]` is a still and is the whole
        corpus until now; a declared animation is `0 .. frames - 1`, and every one of them is its
        own render, its own key and its own product (board:1129)."""
        if self.scene.animation is None:
            return [None]
        return list(range(int(self.scene.animation["frames"]["value"])))

    def output_names(self):
        names = []
        for name in sorted(self.renders):
            for frame in self.frame_grid():
                names.extend(sorted(output_names_for(name, frame).values()))
        return names

    def gltf_names(self):
        """One glTF per subject, and every one of them is imported into the oracle's scene. A
        subject's other files -- the same surface under another index width, the same placement
        through a matrix -- are the RUNNER's to render and score against this one, and none of them
        reaches the oracle."""
        return [s.conversion.settings["outputName"] if s.kind == "blend" else s.entry
                for s in self.subjects]


def output_names_for(recipe_name, frame=None):
    """The float pair a recipe leaves behind: the EXR the score was defined on and the flat f32 dump
    a C++ reader can be twenty lines long for. No picture -- both pictures are the runner's.

    A FRAME IS PART OF THE NAME AND NOT ONLY OF THE KEY. Two frames of one animated case are two
    pictures of the same declaration, so a shared name would leave the store publishing whichever
    was written last -- and the runner, which reads these names, could not stop at the frame that
    failed because it could not tell one from another. `None` is a still and keeps the names the
    corpus already has.
    """
    suffix = "" if recipe_name == DEFAULT_RECIPE_NAME else "." + recipe_name
    if frame is not None:
        suffix += ".f%04d" % frame
    names = {"exr": "oracle" + suffix + ".exr", "raw": "oracle" + suffix + ".raw"}
    # THE QUANTITIES BELONG TO THE DEFAULT RECIPE ONLY, and that is a cost decision with a measured
    # number behind it: a quantity's raw is an uncompressed f32 plane at 14.75 MB, eighteen of them
    # is 265 MB per recipe per case, and the OTHER recipes exist to re-render the PICTURE -- the
    # seed-shift pair proves the beauty is deterministic, the coverage recipe raises its sample
    # count. Nothing reads a second seed's normal, so nothing writes one.
    if recipe_name != DEFAULT_RECIPE_NAME:
        return names
    for quantity in QUANTITY_PASSES:
        names[quantity + "Exr"] = "oracle." + quantity + suffix + ".exr"
        names[quantity + "Raw"] = "oracle." + quantity + suffix + ".raw"
    return names


def _check_names(subjects):
    """One file per name in a case directory, whatever produced it."""
    seen = {}
    for subject in subjects:
        for file in subject.files:
            if file["as"] in RESERVED_OUTPUT_NAMES:
                raise Refusal("manifest.subjects", expected="a name outside the reserved set",
                              observed=file["as"])
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


def _subjects(value):
    subjects = [_Subject(i, v) for i, v in enumerate(value)]
    identifiers = [s.id for s in subjects]
    if len(set(identifiers)) != len(identifiers):
        raise Refusal("manifest.subjects", expected="distinct ids", observed=", ".join(identifiers))
    return subjects


class _Subject:
    def __init__(self, index, field):
        where = "manifest.subjects[%d]" % index
        self.id = field["id"]
        self.kind = field["kind"]
        self.name = field["name"]
        note = licence.check_subject_name(self.name)
        if note:
            print("notice: subject " + self.name + " -- " + note + " -- recorded, not refused",
                  file=sys.stderr)
        self.source = field["source"]
        self.files = field["files"]
        self.entry = field["entry"]
        self.attributes = field.get("attributes", [])
        self.notes = field.get("notes", "")
        # A DECLARED CORRECTION ON TOP OF THE VERIFIED UPSTREAM BYTES, or None. The pin above is
        # untouched by it, which is the whole shape: the fetch still checks upstream's digest.
        self.patch = field.get("patch")
        self.conversion = _Conversion(where, field["conversion"]) if "conversion" in field else None
        for position, file in enumerate(self.files):
            spelling = "%s.files[%d]" % (where, position)
            if "licence" in file:
                for grant in licence.declared_grants(file["licence"]):
                    grantNote = licence.check_spdx(grant["spdx"], spelling + ".licence")

                    if grantNote:

                        print("notice: " + grantNote + " -- recorded, not refused", file=sys.stderr)
            if (file["role"] == "archive-member") != ("member" in file):
                raise Refusal(
                    spelling + ".member",
                    expected="a member path exactly when the role is archive-member",
                    observed=repr(file.get("member")),
                )
        if self.entry not in [f["as"] for f in self.files]:
            raise Refusal(where + ".entry", expected="one of the declared files", observed=self.entry)

    def metadata_file(self):
        for file in self.files:
            if file["role"] == "metadata":
                return file
        return None


class _Scene:
    """What is declared beside the glTF because it must never cross it."""

    def __init__(self, field):
        self.camera = field["camera"]
        self.light = field["light"]
        self.world = field["world"]
        self.material = field["material"]
        # THE GRID THIS CASE IS JUDGED ON, or None for a still. The FRAME is not here: it varies per
        # render and travels in the key beside this, so what a scene declares is the grid and what a
        # product declares is which frame of it.
        self.animation = field.get("animation")
        # WHICH `KHR_materials_variants` VARIANT BOTH SIDES RENDER, or None for the extension's own
        # default (board:1188). It is a NAME, so the oracle resolves it against the file it imported
        # rather than against a position in a list this declaration would have to keep in step with.
        self.material_variant = field.get("materialVariant")
        # THE REFUSAL THAT USED TO LIVE HERE IS GONE BECAUSE ITS CAUSE IS (board:1198). It read
        # `index` must be 0, because Blender's importer makes the file's first animation the active
        # one and a second index would have rendered animation 0 while reporting the other. That was
        # a fact about the IMPORTED ACTION, and no imported action reaches the render any more: the
        # preparer writes an exact key at every rendered frame for every channel of every DECLARED
        # animation, from the file's own accessor bytes. What survives is the narrower claim below --
        # a set that names one animation twice is a declaration that cannot be honoured, because the
        # two copies drive the same node's same path and the format states no result for that.
        if self.animation is not None:
            seen = self.animation["animations"]
            if len(set(seen)) != len(seen):
                raise Refusal("manifest.scene.animation.animations",
                              expected="each animation named once", observed=repr(seen),
                              why="one animation named twice drives every node it touches twice, "
                                  "and glTF states no result when two channels target one node's "
                                  "one path")
        if self.animation is not None and self.animation["frames"]["value"] < 2:
            raise Refusal("manifest.scene.animation.frames",
                          expected="at least 2", observed=repr(self.animation["frames"]["value"]),
                          why="one frame of an animation is a still that renders the pose at t=0 "
                              "and passes every frame-by-frame comparison")
        # THE ONE NUMBER THAT COULD MAKE A DECLARED SUN AN AREA SOURCE, refused here because the sun
        # arm no longer carries a field in which a case could admit the estimator it would get back.
        # The runner refuses the same number when it builds its own light; this refusal is what stops
        # the ORACLE being rendered with a disc in the first place.
        if self.light["kind"] == "sun" and self.light["angleRad"] != 0:
            raise Refusal("manifest.scene.light.angleRad", expected="0",
                          observed=repr(self.light["angleRad"]),
                          why="a sun with an angle is a disc, which is an area source and an "
                              "estimator, and this arm's whole claim is that it has none")

    def as_job(self):
        job = {"camera": self.camera, "light": self.light, "world": self.world, "material": self.material}
        if self.animation is not None:
            job["animation"] = self.animation
        # ABSENT WHEN UNDECLARED AND NOT NULL IN THE KEY: this dictionary is what the oracle's cache
        # key is derived from, so a key that gained a field would miss on every case in the corpus
        # and re-render 37 manifests through Cycles to produce the same bytes.
        if self.material_variant is not None:
            job["materialVariant"] = self.material_variant
        return job


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
    # A DECLARED SUN CAN NO LONGER ANSWER `selected`. The arm declares a single source, the angle is
    # refused where the scene is read, and `no_surface_of_the_subject_is_a_light` takes the subject's
    # own emission out of the light tree and out of every gathering ray -- so the word left the sun
    # arm's enumeration rather than staying spellable. What the file's OWN lights bring is a
    # different question and the `gltf` arm keeps it.
    reduced = (material.get("kind") in ("emission", "emission-per-material",
                                       "emission-by-material-index") or
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
    if DEFAULT_RECIPE_NAME not in value:
        raise Refusal(
            "manifest.renders",
            expected="a recipe named " + DEFAULT_RECIPE_NAME,
            observed=", ".join(sorted(value)),
            why="the acceptance numbers are judged on one named recipe, and it is that one",
        )
    for name in sorted(value):
        if not RECIPE_NAME.match(name):
            raise Refusal("manifest.renders." + name, expected="lowercase, digits and hyphens", observed=name)
        _recipe("manifest.renders." + name, value[name])
    return value


def _recipe(where, field):
    """What makes an oracle render mean anything, and each of these is refused for its own reason."""
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
    if field["pixelFilter"]["widthPx"] < 0.01:
        raise Refusal(where + ".pixelFilter.widthPx", expected=">= 0.01, the RNA minimum",
                      observed=field["pixelFilter"]["widthPx"])
    return field


def _identical_coverage(value, subjects):
    """Files this case's own render must land in the same pixels as -- decided between two renders
    of ours, with no oracle in it at all, so the agreement is exact rather than within a tolerance.
    Each has to be a file this manifest already declares, so a claim cannot name a subject nothing
    prepares."""
    if value is None:
        return []
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


class _Conversion:
    def __init__(self, where, field):
        self.settings = field
        if field["outputName"] in RESERVED_OUTPUT_NAMES:
            raise Refusal(
                where + ".conversion.outputName",
                expected="a name outside the reserved set",
                observed=field["outputName"],
            )
        # One file per conversion, so the whole product has one hash and one name in the store.
        # GLTF_SEPARATE writes a .bin and a texture directory beside the .gltf, and a store keyed on
        # the .gltf alone loses them -- measured: the import then fails on a missing scene.bin.
        if field["exportSettings"].get("export_format") != "GLB":
            raise Refusal(
                where + ".conversion.exportSettings.export_format",
                expected="GLB",
                observed=repr(field["exportSettings"].get("export_format")),
                why="a multi-file export cannot be one entry in a content store keyed by hash = filename",
            )
