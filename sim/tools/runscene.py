#!/usr/bin/env python3
"""Ein deklarierter Lauf, von einem Werkzeug aus gefahren.

`gpu_walk` nimmt zwei Woerter: welcher Mod, welche Szene. Ein Werkzeug, das eine Szene ERZEUGT --
eine Kachel je Ort, eine Geschwindigkeitsstufe je Lauf -- schreibt sie darum als Ein-Szenen-Mod in
ein Wegwerfverzeichnis und zeigt OUTSHINE_MODS dorthin. Die Sprache bleibt JSON; nur die Wurzel ist
Umgebung.

Die drei Umgebungswerte und warum jeder einer ist, stehen in doc/build-and-ops.md.
"""
import json
import os
import pathlib
import subprocess
import tempfile

SIM = pathlib.Path(__file__).resolve().parent.parent


def run(binary, scene, out_root=".", tiles=None, env=None, cwd=None, capture=True):
    """scene: das Szenenobjekt (dict) inklusive `id`. Gibt CompletedProcess zurueck."""
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
    """Der haeufigste Fall: ein Frame, ein PNG. Bewegung ist die Normalform, ein Standbild ihr
    Sonderfall mit einem Frame und ohne Kanal (clients/Scene.h)."""
    s = {"id": sid, "kind": "run", "lat": lat, "lon": lon, "eyeM": 1.70,
         "yawDeg": 0, "pitchDeg": 0, "fovDeg": 60, "utc": "2026-06-21T11:00:00Z",
         "windDeg": 250, "windMs": 2.0, "cloudCover": 0.0,
         "capture": {"width": 1280, "height": 720},
         "runs": [{"kind": "motion", "frames": 1, "give": "stills", "path": out}]}
    depth = kw.pop("depth", None)
    cap = kw.pop("capture", None)
    if cap:
        s["capture"].update(cap)
    if depth:
        s["runs"][0]["depth"] = depth
    s.update(kw)
    return s
