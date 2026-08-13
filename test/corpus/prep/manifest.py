"""The manifest, read strictly: an unknown key is a refusal, not a shrug.

WHAT A MANIFEST MAY CONTAIN IS NOT HERE. It is `test/corpus/manifest-schema.json`, which the render
runner reads too, so a key exists once. What is here is the POLICY this preparer owns: which Cycles
settings make an oracle render meaningless, which acceptance a reduced scene owes, and which names
belong to the test that reads the case directory.
"""

import json
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
        licence.check_subject_name(self.name)
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
                licence.check_spdx(file["licence"]["spdx"], spelling + ".licence")
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
        return {"camera": self.camera, "light": self.light, "world": self.world, "material": self.material}


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
