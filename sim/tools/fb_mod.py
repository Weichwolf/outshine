"""Where the content is, for the python side. Same mod.json the Makefile and src/missions/FBMod.h read
(doc/mods.md §3): the engine names no aircraft, no mesh and no mission directory of its own.

Override with FB_MOD=<dir> (relative to sim/, or absolute)."""
import json
import os

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_DIR = os.path.dirname(SIM_DIR)
MOD_DIR = os.path.abspath(os.path.join(SIM_DIR, os.environ.get("FB_MOD", "../mods/f16")))

with open(os.path.join(MOD_DIR, "mod.json"), encoding="utf-8") as f:
    MANIFEST = json.load(f)

ID = MANIFEST["id"]


def root(key):
    return os.path.join(MOD_DIR, MANIFEST[key])


AIRCRAFT = root("aircraft")
MODELS = root("models")
MISSIONS = root("missions")
CAMPAIGNS = root("campaigns")
DATA = root("data")

# For `git status --porcelain <paths>`: repo-relative, which is what git wants.
GUARD_PATHS = [os.path.relpath(p, REPO_DIR) for p in (MISSIONS, AIRCRAFT)] + \
              [os.path.relpath(os.path.join(AIRCRAFT, "MODEL-DELTAS.md"), REPO_DIR)]


def mission(name):
    """A bare name resolves in the mod; anything ending in .fbm is already a path."""
    return name if name.endswith(".fbm") else os.path.join(MISSIONS, name + ".fbm")
