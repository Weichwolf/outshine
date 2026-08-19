"""The glTF-Asset-Generator's animation groups, derived from the pin rather than chosen (board:1458).

WHAT THIS PRODUCES IS A DECLARATION AND NOT A PRODUCT. One directory per upstream model, each holding a
manifest that names the pin, the files and what the model is about -- the same shape the picture corpus
already uses, so one enumeration reaches the runner and the viewer with nothing new to teach either.

IT SELECTS NOTHING. Every `.gltf` in a named group arrives; which of them this engine can pose is a
question the render suite answers, and a `grep` for a feature name here would answer a different one.

THE CRITERION IS READ FROM UPSTREAM AND NOT WRITTEN HERE. Each group publishes a readme whose markdown
table carries one row per model naming exactly what varies -- `Target: Translation, Interpolation: Step`
-- so a case's own statement of what it is about is the upstream's, quoted, and thirty-four cases are a
tool's work rather than thirty-four writing tasks.

THE CAMERA IS DERIVED AND NOT QUOTED. These models carry none, and the framing rule takes the union of
world bounds over the declared frames -- which means posing the hierarchy and sampling the animation. A
Python importer computing that would be a second spelling of the flattener and the sampler, so the
manifest declares `camera.source: derived` and the preparer computes it inside Blender, where the scene
is already imported and posed. `prep/in_blender_render.py` publishes what it derived and the runner
reads it: one derivation, used by both sides by construction.

TWO FRAMES AND NOT A SEQUENCE. Cycles renders are not cached, so an eight-frame motion over thirty-four
cases would be three times the whole corpus for one extension. What a still can decide is where every
vertex ENDS UP when the pose is applied, which is exactly what more than four joint influences, a hole
in a joint chain, one skin over several meshes and skin-plus-node animation together get wrong.
"""

import hashlib
import json
import os
import re
import struct
import urllib.parse
import urllib.request

from .refusal import Refusal

RAW = "https://raw.githubusercontent.com/KhronosGroup/glTF-Asset-Generator/"
TREE = "https://api.github.com/repos/KhronosGroup/glTF-Asset-Generator/git/trees/"
BLOB = "https://github.com/KhronosGroup/glTF-Asset-Generator/blob/"
ROOT = "Output/Positive"

# THE FIVE GROUPS THAT ANIMATE, named because the repository holds twenty-six and the other twenty-one
# are about materials, meshes and textures this corpus reaches by other cases. `Animation_SamplerType`
# carries the interpolation kinds and belongs with the other four.
GROUPS = ("Animation_Node", "Animation_NodeMisc", "Animation_SamplerType", "Animation_Skin",
          "Animation_SkinType")

# THE PROJECT'S OWN LICENCE, and it is declared here because this vendor publishes no per-model
# metadata for it to be derived from -- which is the arm `_check_licences` takes for every kind that is
# not `khronos-sample-assets`.
LICENCE = [{"spdx": "Apache-2.0", "holder": "The Khronos Group Inc.",
            "covers": "Everything",
            "statedAt": "https://github.com/KhronosGroup/glTF-Asset-Generator/blob/main/LICENSE"}]

# ONE ROW OF A GROUP'S TABLE: the model number, then the cells that say what it varies.
ROW = re.compile(r"^\|\s*\[(\d+)\]\([^)]*\)(?:<br>\[View\]\([^)]*\))?\s*\|(.*)\|\s*$", re.M)
HEADER = re.compile(r"^\|\s*\|(.*)\|\s*$", re.M)
CELL_LINK = re.compile(r"\[([^\]]*)\]\([^)]*\)")
CELL_IMAGE = re.compile(r"<img[^>]*>")


def _get(url, binary=False):
    with urllib.request.urlopen(url, timeout=60) as response:
        raw = response.read()
    return raw if binary else raw.decode("utf-8", "replace")


def models_in(commit, group):
    """Every `.gltf` the group holds at the pin, with the files each one needs, exhaustively."""
    listing = json.loads(_get(TREE + commit + ":" + (ROOT + "/" + group).replace("/", "%2F") +
                              "?recursive=1"))
    if listing.get("truncated"):
        raise Refusal("the listing of " + group,
                      expected="the whole directory",
                      observed="a truncated tree",
                      why="a truncated enumeration is not an enumeration, and a selection drawn "
                          "from one is a curation nobody declared")
    blobs = set(entry["path"] for entry in listing["tree"] if entry["type"] == "blob")
    out = []
    for path in sorted(blobs):
        if not path.endswith(".gltf") or "/" in path:
            continue
        stem = path[:-len(".gltf")]
        # **WHAT A MODEL NEEDS IS WHAT IT SAYS IT NEEDS, read from the file rather than guessed from a
        # name.** These models reference their buffers and images by URI, and the images sit in a
        # `Textures/` directory beside them -- a prefix match over the listing would have found the
        # buffer and silently missed every texture, which is a case that renders untextured and says
        # nothing about it.
        document = json.loads(_get(RAW + commit + "/" + ROOT + "/" + group + "/" + path))
        needed = [path]
        for section in ("buffers", "images"):
            for entry in document.get(section, []):
                uri = entry.get("uri")
                if uri is None or uri.startswith("data:"):
                    continue
                reference = urllib.parse.unquote(uri)
                if reference not in blobs:
                    raise Refusal("the model " + stem,
                                  expected="the file it names, " + reference,
                                  observed="nothing at that path in the group at the pin",
                                  why="a model whose own references do not resolve is not a case")
                if reference not in needed:
                    needed.append(reference)
        # **A MATERIAL WITH NO NAME IS KEYED THE WAY THE ORACLE NAMES IT.** These models leave
        # `materials[].name` out, and Blender's glTF importer calls an unnamed material `Material_<i>`.
        # That is an assumption about the oracle's importer and it is SELF-CHECKING: the material arm
        # refuses by name when the scene carries a material the manifest does not declare, so a changed
        # convention stops the preparation rather than rendering something nobody declared.
        buffers = [_get(RAW + commit + "/" + ROOT + "/" + group + "/" +
                        urllib.parse.unquote(entry["uri"]), binary=True)
                   for entry in document.get("buffers", []) if entry.get("uri")]
        moves, ends = motion_of(document, buffers)
        # **A FILE THAT CARRIES AN ANIMATION DECLARES ONE EVEN WHEN IT CANNOT MOVE** (board:1458).
        # Blender applies what the file carries; this engine poses what the MANIFEST declares -- so a
        # case that stayed silent about a constant channel put the oracle on the animated pose and us
        # on the rest pose, and the two were comparing different bodies. [MEASURED]
        # `Animation_NodeMisc_03` keys one frame at [-0.1, 0, 0] and its vertex 6 then sat inside the
        # camera's near plane; `_05` overrides a rotation to a constant and disagreed outright.
        #
        # WHAT MOVING DECIDES IS THE GRID'S LENGTH AND NOT ITS EXISTENCE: two frames where the pose
        # changes, one where it cannot.
        animates = len(document.get("animations", [])) > 0
        moves = moves and ends > 0.0
        names = [material.get("name") or ("Material_" + str(at))
                 for at, material in enumerate(document.get("materials", []))]
        out.append((stem, needed, names, animates, moves, ends))
    return out


def _cells(text):
    text = CELL_IMAGE.sub("", text)
    text = CELL_LINK.sub(r"\1", text)
    return [cell.strip() for cell in text.split("|")]


def says(commit, group):
    """What each model of the group varies, read from the group's own table.

    The table's first column is the model and the rest are the properties it sets, so a row becomes
    `Target: Translation, Interpolation: Step` -- the upstream's statement, quoted rather than
    paraphrased, which is what makes the criterion `self-describing` and not ours.
    """
    text = _get(RAW + commit + "/" + ROOT + "/" + group + "/README.md")
    heading = HEADER.search(text)
    names = _cells(heading.group(1)) if heading else []
    stated = {}
    for match in ROW.finditer(text):
        values = _cells(match.group(2))
        pairs = []
        for at, value in enumerate(values):
            label = names[at] if at < len(names) else ""
            if not value or label in ("", "Sample Image"):
                continue
            pairs.append((label + ": " + value) if label else value)
        stated[match.group(1)] = ", ".join(pairs)
    return stated


def _fetched(commit, group, name, payload):
    return {
        "url": RAW + commit + "/" + ROOT + "/" + group + "/" + name,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "bytes": len(payload),
        "as": name,
        "role": ("gltf" if name.endswith(".gltf")
                 else "buffer" if name.endswith(".bin") else "image"),
        "licence": LICENCE,
    }


def _accessor(document, buffers, index):
    """One accessor's values, read out of the buffer the file names."""
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    payload = buffers[view.get("buffer", 0)]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    wide = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[accessor["type"]]
    return [struct.unpack_from("<%df" % wide, payload, start + at * 4 * wide)
            for at in range(accessor["count"])]


def motion_of(document, buffers):
    """Whether the animation can move the subject at all, and the last instant it is keyed at.

    **BOTH ARE READ FROM THE FILE AND NEITHER IS ASSUMED** (board:1458). Four models of this corpus
    proved each assumption wrong in turn: `Animation_NodeMisc_03` carries ONE keyframe, `_05` keys two
    that are the same value, and `_01` and `_02` do not start at zero. A case that declares a grid and
    cannot move is a still rendered twice, which agrees with the oracle by construction -- and the
    sequence check is right to refuse it.
    """
    ends, moves = [], False
    for animation in document.get("animations", []):
        for sampler in animation["samplers"]:
            times = _accessor(document, buffers, sampler["input"])
            values = _accessor(document, buffers, sampler["output"])
            if times:
                ends.append(max(time[0] for time in times))
            if len(set(values)) > 1:
                moves = True
    return moves, (max(ends) if ends else 0.0)


# THE GRID EVERY CASE IS DECIDED ON, and the rate is read from the corpus rather than assumed.
#
# [MEASURED] the generator keys its animations at **0, 1 and 2 seconds** -- not at one second, which is
# what this constant first said. At 2 fps the grid was 0 s and 0.5 s, and for a STEP interpolation a
# step holds until its next key, so both samples were the same pose: `Animation_Node_03` reported *0
# frames whose picture differs from frame 0* and was right to.
#
# [SET] 1 fps and two frames, so the grid is 0 s and 1 s -- two DISTINCT keyframes, which differ under
# every interpolation the corpus carries. The period is 2 s and its endpoints are 0 s and 2 s, so this
# grid is not `AnimatedTriangle`'s trap of comparing a pose with itself across a full period.
FPS = 1
FRAMES = 2

RECIPE = {
    "engine": "CYCLES", "device": "METAL", "resolutionX": 1280, "resolutionY": 720,
    "samples": 1, "adaptiveSampling": False, "denoise": False, "seed": 0,
    "pixelFilter": {"type": "BOX", "widthPx": 0.01},
    "filmTransparent": True, "filmExposure": 1.0, "scaleLength": 1.0,
    "bounces": {"max": 0, "diffuse": 0, "glossy": 0, "transmission": 0, "volume": 0,
                "transparentMax": 0},
    "colourManagement": {"displayDevice": "sRGB", "viewTransform": "Standard", "look": "None",
                         "exposure": 0.0, "gamma": 1.0},
    "exrCodec": "ZIP",
    "note": "The binary mask and the emitted colour in one render, at one sample per pixel with the "
            "box filter at its RNA minimum -- so Cycles has no integration left to perform and a "
            "facet returns exactly what it emits. What this case decides is WHERE the geometry is "
            "after the pose is applied, and a recipe that shaded would put a second question in the "
            "same picture.",
}


# [SET] WHAT EVERY MATERIAL EMITS. One colour for all of them, because what a pose case decides is
# WHERE the geometry is: two colours would put a second question -- which material a facet wears -- in
# the same picture, and that question has its own cases.
EMISSION = [0.85, 0.15, 0.15]


def case(commit, group, name, files, materials, animates, moves, ends, stated, pinned_on,
         pin_reason, measured=None):
    """One upstream model as a case.

    `measured` is the frame fraction a previous run recomputed, preserved across regenerations: it is a
    MEASUREMENT and this importer cannot know it -- the runner recomputes it on every run and refuses a
    mismatch, so what the manifest does is PIN it. A case that has never been run declares zero and
    says so, and the first run's own number replaces it.
    """
    payloads = {path: _get(RAW + commit + "/" + ROOT + "/" + group + "/" + path, binary=True)
                for path in files}
    number = name.rsplit("_", 1)[-1]
    what = stated.get(number, "")
    return {
        "schema": "outshine/render-oracle-manifest",
        "schemaVersion": 1,
        "id": "render/animation/" + name.lower().replace("_", "-"),
        "title": "Khronos " + name + " -- " + (what if what else "an animation of the generator's " +
                                               group + " group"),
        "covers": ["khronos:" + name, "board:1458"],
        "criterion": {
            "kind": "self-describing",
            "oracleRole": "reference",
            "says": what if what else "one animation of the " + group + " group",
            "statedAt": BLOB + commit + "/" + ROOT + "/" + group + "/README.md",
            "note": "WHAT THIS CASE DECIDES IS A POSE. The generator states per model what it varies "
                    "and this case quotes that; what it claims is that every vertex lands where the "
                    "declared animation puts it at each frame of the grid, which is what a still can "
                    "decide. Smoothness between the frames is a moving camera's question and belongs "
                    "to the scenario suite.",
        },
        "subjectClass": "opaque-min-1px",
        "acceptanceClass": {
            "is": "general-position",
            "because": "The framing rule's own direction over a subject that MOVES, so the comparison "
                       "is taken at every frame of the declared grid rather than at one pose. Nothing "
                       "here is analytic: what is compared is which pixels each side covers at each "
                       "sampled time and what colour each body wears there.",
        },
        "blender": {
            "version": "5.2.0",
            "note": "A release version, not a commit. Recorded so a red rung can be attributed to our "
                    "renderer or to the oracle, and carried in the oracle's cache key so a version "
                    "bump misses instead of serving a stale render.",
        },
        "subjects": [{
            "id": name.lower().replace("_", "-"),
            "kind": "gltf",
            "name": name,
            "entry": name + ".gltf",
            "attributes": ["POSITION"],
            "notes": "One model of the generator's " + group + " group, taken whole: the `.gltf` and "
                     "every buffer it names.",
            "source": {"kind": "khronos-asset-generator", "commit": commit, "model": group + "/" + name,
                       "pinnedOn": pinned_on, "pinReason": pin_reason},
            "files": [_fetched(commit, group, path, payloads[path]) for path in files],
        }],
        "scene": {
            "frame": "gltf",
            "camera": {
                "source": "derived",
                "note": "The generator's models carry no camera, and the framing rule takes the union "
                        "of world bounds over the declared frames -- which means posing the hierarchy "
                        "and sampling the animation. It is derived inside Blender, where the scene is "
                        "already imported and posed, and the runner reads what was derived.",
            },
            "light": {"kind": "none",
                      "note": "The material emits, so nothing has to arrive for the picture to exist."},
            "world": {"kind": "uniform", "colourLinear": [1.0, 1.0, 1.0], "strength": 0.0,
                      "note": "Declared and zero rather than left factory, so the picture is a "
                              "function of this manifest."},
            "material": {
                "source": "manifest",
                "kind": "emission-per-material",
                # `<default>` IS DECLARED ONLY WHERE A PRIMITIVE SELECTS NO MATERIAL, because the
                # arm refuses in BOTH directions: a colour for a material the scene does not carry is
                # as much a defect as a material the manifest does not declare.
                "colourLinearPerMaterial": ({material: EMISSION for material in materials}
                                            if materials else {"<default>": EMISSION}),
                "note": "Emission, one colour per glTF material, keyed by the name the FILE gives it. "
                        "A flat emitter is what isolates WHERE the geometry is from what it is made "
                        "of, which is the only question a pose case asks.",
            },
            **({"animation": {
                "animations": [0],
                "fps": {"value": (2.0 / ends) if moves and ends > 0.0 else 1.0,
                        "unit": "frames per second", "origin": "derived",
                        "derivation": ("2 / " + repr(ends) + " s, so the two-frame grid runs 0 s and "
                                       + repr(ends / 2.0) + " s -- the MIDDLE of the span this file "
                                       "keys, not its end." if moves and ends > 0.0 else
                                       "one, because the grid is one frame: this file's channels "
                                       "cannot change the pose, so a rate decides nothing."),
                        "note": "THE RATE IS THE ANIMATION'S OWN AND NOT A CONSTANT. [MEASURED] the "
                                "generator keys at 0, 1 and 2 seconds in some models and at 2 to 6 "
                                "in others, and four of them do not start at zero at all -- so a "
                                "fixed rate sampled two points inside one step of `Animation_Node_03` "
                                "and never reached the channels of `Animation_NodeMisc_02`.\n\n"
                                "FRAME 1 LANDS IN THE MIDDLE OF THE SPAN AND NOT AT ITS END, because "
                                "an END is where a rotation comes back: `Animation_NodeMisc_01` turns "
                                "through a full circle between its first and last key, so the two "
                                "endpoints are one pose and the grid compared it with itself."},
                "frames": {"value": FRAMES if moves else 1, "unit": "frames", "origin": "derived",
                           "derivation": ("two, because the file's channels change the pose"
                                          if moves else
                                          "one, because the file carries an animation whose channels "
                                          "cannot change the pose -- a constant override or a single "
                                          "keyframe. It is DECLARED so that both sides pose the same "
                                          "body; a case that stayed silent put the oracle on the "
                                          "animated pose and this engine on the rest pose."),
                           "note": "Frame 0 and one INSIDE the motion. What a still decides is where "
                                   "every vertex ends up when the pose is applied; Cycles renders are "
                                   "not cached, so a sequence over thirty-four cases would cost three "
                                   "times the whole corpus for one extension."},
            }} if animates else {}),
        },
        "renders": {"default": RECIPE, "seed-shift": dict(RECIPE, seed=1)},
        "expected": {
            "subjectFrameFraction": {
                "value": measured if measured is not None else 0.0,
                "unit": "dimensionless",
                "origin": "measured" if measured is not None else "SET",
                "note": ("The fraction of the frame the subject's projected area covers at frame 0. "
                         "RECOMPUTED BY THE RUNNER ON EVERY RUN and refused on mismatch, so what this "
                         "line does is pin it -- a DRIFT DETECTOR rather than a present-error check, "
                         "honest because the camera is derived from the framing rule and not chosen."
                         if measured is not None else
                         "UNMEASURED. This case has not been run, so the runner's own recomputation is "
                         "what will replace this zero; until then the frame-fraction metric reports the "
                         "whole fraction as its error and says the number is not yet pinned."),
            }
        },
    }


def measured_fraction(root, name):
    """The frame fraction a previous generation pinned, or None where the case is new."""
    path = os.path.join(root, name, "manifest.json")
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        declared = json.load(f)
    stated = declared.get("expected", {}).get("subjectFrameFraction", {})
    return stated.get("value") if stated.get("origin") == "measured" else None


def write(declaration, root, name):
    directory = os.path.join(root, name)
    os.makedirs(directory, exist_ok=True)
    path = os.path.join(directory, "manifest.json")
    with open(path, "w") as f:
        json.dump(declaration, f, indent=2)
        f.write("\n")
    return path
