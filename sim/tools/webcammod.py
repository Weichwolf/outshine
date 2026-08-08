#!/usr/bin/env python3
"""Der Webcam-Mod als Tabelle. `mods/webcams/mod.json` IST die Erklaerung -- Standpunkt, Pose,
Aufnahme und Lauf je Kamera. Die Werkzeuge lesen ihn hier und schreiben nur das eine zurueck, was
sich je Lauf aendert: den Zeitstempel des Livebildes."""
import json
import pathlib

MOD = pathlib.Path(__file__).resolve().parent.parent.parent / "mods" / "webcams" / "mod.json"


def load():
    return json.loads(MOD.read_text())


def cams(mod=None):
    """Eine flache Zeile je LIVE-Szene: Szenenfelder plus der `pose`-Block, plus `slug`."""
    m = mod or load()
    out = []
    for s in m["scenes"]:
        if s["id"].endswith("-fit"):
            continue
        row = {k: v for k, v in s.items() if k not in ("pose", "runs", "capture")}
        row.update(s.get("pose", {}))
        row["slug"] = s["id"]
        row["altM"] = s["lensAslM"]
        out.append(row)
    return out


def scene(mod, sid):
    for s in mod["scenes"]:
        if s["id"] == sid:
            return s
    return None


def save(mod):
    MOD.write_text(json.dumps(mod, indent=2, ensure_ascii=False) + "\n")


SCENE_KEYS = ("lat", "lon", "eyeM", "lensAslM", "yawDeg", "pitchDeg", "fovDeg",
              "windDeg", "windMs", "cloudCover", "utc")


def apply(mod, slug, fields):
    """Schreibt eine flache Zeile zurueck: Szenenfelder in die Szene, alles andere in `pose`.
    Aendert sich `fitImage`, wird die utc der zugehoerigen -fit-Szene daraus neu abgeleitet."""
    live = scene(mod, slug)
    if live is None:
        return
    for k, v in fields.items():
        if k in ("slug", "altM"):
            continue
        (live if k in SCENE_KEYS else live.setdefault("pose", {}))[k] = v
    fit = scene(mod, slug + "-fit")
    if fit is not None:
        for k in ("yawDeg", "pitchDeg", "fovDeg"):
            if k in fields:
                fit[k] = fields[k]
        fit.setdefault("pose", {}).update(
            {k: v for k, v in fields.items() if k not in SCENE_KEYS and k not in ("slug", "altM")})
        if "fitImage" in fields:
            fit["utc"] = fit_utc(fields["fitImage"])


def fit_utc(path):
    """Der Archivpfad von foto-webcam.eu traegt ORTSZEIT der Kamera; die Szene traegt UTC."""
    import datetime
    import zoneinfo
    y, mo, d, hm = path.split("/")
    t = datetime.datetime(int(y), int(mo), int(d), int(hm[:2]), int(hm[2:]),
                          tzinfo=zoneinfo.ZoneInfo("Europe/Berlin")).astimezone(datetime.timezone.utc)
    return t.strftime("%Y-%m-%dT%H:%M:%SZ")
