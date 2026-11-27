"""Reference cameras; the elevated comparison reads the native scenario pose."""

import json
from pathlib import Path
from xml.etree.ElementTree import parse

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def load_catalog():
    config = json.loads((HERE / "places.json").read_text())
    for place in config["places"]:
        if "scenario" in place:
            scenario = parse(ROOT / place["scenario"])
            view = scenario.find("views/view")
            at = view.find("at")
            if at.get("samplesHeight") != "no":
                raise ValueError(
                    "absolute geodetic height required for the comparison camera"
                )
            place.update(
                latitude=float(at.attrib["lat"]),
                longitude=float(at.attrib["lon"]),
                absolute_eye_m=float(at.attrib["heightM"]),
                heading_deg=float(at.attrib["bearingDeg"]),
                pitch_deg=float(at.attrib["pitchDeg"]),
                vertical_fov_deg=float(view.attrib["fovDeg"]),
                utc=scenario.find("clock").attrib["start"],
            )
    return config


def selected_views(config, names):
    if names:
        return [place for place in config["places"] if place["id"] in names]
    return [place for place in config["places"] if "absolute_eye_m" not in place]
