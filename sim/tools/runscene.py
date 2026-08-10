#!/usr/bin/env python3
"""A declared run, driven from a tool.

`gpu_walk` takes two words: which mod, which scene. A tool that GENERATES a scene -- one tile per
place, one speed stage per run -- therefore writes it as a one-scene mod into a throwaway directory
and points OUTSHINE_MODS at it. The language stays JSON; only the root is environment.
"""
import json
import os
import pathlib
import subprocess
import tempfile

SIM = pathlib.Path(__file__).resolve().parent.parent


def run(binary, scene, out_root=".", tiles=None, env=None, cwd=None, capture=True):
    """scene: the scene object (dict) including its `id`. Returns a CompletedProcess."""
    with tempfile.TemporaryDirectory(prefix="outshine-mod-") as root:
        mod = pathlib.Path(root) / "scratch"
        mod.mkdir()
        (mod / "mod.json").write_text(json.dumps(
            {"schema": "outshine/mod/1", "name": "scratch", "scenes": [scene]}, indent=2))
        e = dict(os.environ)
        e["OUTSHINE_MODS"] = root
        e["OUTSHINE_OUT"] = str(out_root)
        if tiles:
            e["OUTSHINE_TILES"] = tiles
        e["OUTSHINE_BUILD"] = build_id(binary)
        if env:
            e.update(env)
        return subprocess.run([str(pathlib.Path(binary).resolve()), "scratch", scene["id"]],
                              capture_output=capture, text=True, env=e,
                              cwd=str(cwd or SIM))


def build_id(binary):
    import hashlib
    return hashlib.md5(pathlib.Path(binary).read_bytes()).hexdigest()


def still(sid, lat, lon, out, **kw):
    """The commonest case: one frame, one PNG. Motion is the normal form and a still its special
    case, with one frame and no channel (clients/Scene.h).

    Without `render=` the declared 1280x720 applies; anything else has to state its reason in the
    same object -- the engine refuses one that does not (clients/Scene.cpp)."""
    s = {"id": sid, "kind": "run", "lat": lat, "lon": lon, "eyeM": 1.70,
         "yawDeg": 0, "pitchDeg": 0, "fovDeg": 60, "utc": "2026-06-21T11:00:00Z",
         "windDeg": 250, "windMs": 2.0, "cloudCover": 0.0,
         "runs": [{"kind": "motion", "frames": 1, "give": "stills", "path": out}]}
    depth = kw.pop("depth", None)
    render = kw.pop("render", None)
    if render:
        s["render"] = render
    if depth:
        s["runs"][0]["depth"] = depth
    s.update(kw)
    return s
