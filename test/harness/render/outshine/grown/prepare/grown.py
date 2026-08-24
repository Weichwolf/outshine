"""The parts the ENGINE grows, produced by running the engine.

A generator is C++ and a `.glb` has to exist before Blender opens, so the preparer builds the
library and runs `test/harness/render/outshine/grown/GrowPart.cpp` over it -- exactly as it locates and runs Blender. It is
the same door `CLAUDE.md` opens for this script and the only reading of it that keeps the constraint:
growing a tree in Python here would be a second implementation of the generator, and the number the
render case then published would be about that implementation and not about the engine's.

WHAT IS CACHED IS NOTHING. The recipe is a few hundred bytes of manifest and the product a few tens
of kilobytes, and the generator's own version is the source tree -- a store key that failed to cover
it would serve a part the current code no longer grows, which is the one staleness a cache cannot see
from inside (board:0105). Rebuilding is a second and the compile is incremental.
"""

import json
import os
import subprocess
import tempfile

from prep.refusal import Refusal

_HERE = os.path.dirname(os.path.abspath(__file__))
def _repository(start):
    at = start
    while at != os.path.dirname(at):
        if os.path.isdir(os.path.join(at, "src", "assets")) and os.path.isfile(os.path.join(at, "Makefile")):
            return at
        at = os.path.dirname(at)
    raise Refusal("locating the repository", "a parent of " + start + " holding src/assets and a Makefile",
                  "no parent of " + start + " holds both")

REPOSITORY = _repository(_HERE)
SOURCE = os.path.join(_HERE, "GrowPart.cpp")
SPECIES_DIRECTORY = os.path.join(REPOSITORY, "src", "assets", "world", "species")
LIBRARY = os.path.join(REPOSITORY, "build", "liboutshine.a")

INCLUDES = ("src/core", "src/gltf", "src/generators", "src/generators/draw", "src/clients")

SHAPES = ("grown-tree",)

def build(where, shape, parameters):
    """The bytes of one grown part, and the report the generator printed about it."""
    if shape != "grown-tree":
        raise Refusal(where + ".shape", expected="one of " + ", ".join(SHAPES), observed=shape)
    species = _text(where + ".parameters.species", parameters, "species")
    node = _text(where + ".parameters.node", parameters, "node")
    budget = _fraction(where + ".parameters.pixelHeightFrac", parameters)
    unknown = sorted(set(parameters) - {"species", "node", "pixelHeightFrac"})
    if unknown:
        raise Refusal(where + ".parameters." + unknown[0],
                      expected="one of node, pixelHeightFrac, species", observed=unknown[0],
                      why="a parameter nobody reads is a part that silently did not change")

    declaration = os.path.join(SPECIES_DIRECTORY, species + ".json")
    if not os.path.isfile(declaration):
        raise Refusal(where + ".parameters.species", expected="a species declared under " +
                      SPECIES_DIRECTORY, observed=species)

    binary = _built()
    with tempfile.TemporaryDirectory(prefix="outshine-grown-") as scratch:
        product = os.path.join(scratch, "part.glb")
        run = subprocess.run(
            [binary, "--species", declaration, "--node", node,
             "--pixel-height-frac", repr(budget), "--out", product],
            capture_output=True, text=True, check=False, cwd=REPOSITORY)
        if run.returncode != 0:
            raise Refusal(where, expected="the generator to grow the part",
                          observed=(run.stderr or run.stdout).strip())
        with open(product, "rb") as grown:
            produced = grown.read()
    return produced, json.loads(run.stdout)

def _built():
    """The library, then the one program over it. Both are the compiler's own idea of up to date."""
    compiler = os.environ.get("CXX", "c++")
    make = subprocess.run(["make"], cwd=REPOSITORY, capture_output=True, text=True, check=False)
    if make.returncode != 0:
        raise Refusal("make", expected="the library to build",
                      observed=(make.stderr or make.stdout).strip())
    where = os.path.join(tempfile.gettempdir(), "outshine-corpus")
    os.makedirs(where, exist_ok=True)
    binary = os.path.join(where, "GrowPart")
    command = [compiler, SOURCE, "-std=c++23", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
               "-Wno-unused-parameter"]
    command += ["-I" + os.path.join(REPOSITORY, part) for part in INCLUDES]
    command += [LIBRARY, "-o", binary]
    compiled = subprocess.run(command, cwd=REPOSITORY, capture_output=True, text=True, check=False)
    if compiled.returncode != 0:
        raise Refusal("compiling " + os.path.relpath(SOURCE, REPOSITORY),
                      expected="a program that runs the generator",
                      observed=(compiled.stderr or compiled.stdout).strip())
    return binary

def _text(where, parameters, key):
    value = parameters.get(key)
    if not isinstance(value, str) or not value:
        raise Refusal(where, expected="a non-empty name", observed=repr(value))
    return value

def _fraction(where, parameters):
    value = parameters.get("pixelHeightFrac")
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value < 0.0:
        raise Refusal(where, expected="the model length of one pixel as a fraction of the tree's "
                      "height, at least 0", observed=repr(value))
    return float(value)
