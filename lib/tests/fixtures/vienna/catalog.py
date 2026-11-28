"""Reference cameras; the elevated comparison reads the native scenario pose."""

import json
from pathlib import Path
from xml.etree.ElementTree import parse

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def load_catalog():
    config = json.loads((HERE / "places.json").read_text())
    config["source_pins"] = json.loads((HERE / "sources.json").read_text())
    recipe = json.loads((HERE / "water_controls.json").read_text())
    if recipe["source_snapshot"] != config["source_snapshot"]:
        raise ValueError("Water controls require the pinned vector snapshot")
    config["water_controls"] = recipe["water_controls"]
    config["source_cache"] = str(ROOT / "build/vienna-sources")
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
        return [place for place in available_views(config) if place["id"] in names]
    return [place for place in config["places"] if "absolute_eye_m" not in place]


def available_views(config):
    places = {place["id"]: place for place in config["places"]}
    return [
        *places.values(),
        *(
            places[view["origin"]] | view if "origin" in view else view
            for view in config.get("comparison_views", [])
        ),
    ]
