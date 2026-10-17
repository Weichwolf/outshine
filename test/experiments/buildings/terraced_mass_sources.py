"""Existing real Paris controls; no duplicate geometry or synthetic provider tags."""

import json
from pathlib import Path

import shapely


def sources():
    data = json.loads(
        Path(__file__).with_name("data").joinpath("stepped-profiles.json").read_text()
    )
    return [
        row | {"polygon": shapely.from_wkb(row["footprint_wkb"])}
        for row in data["cases"]
    ]
