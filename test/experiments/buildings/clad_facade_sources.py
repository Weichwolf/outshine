"""Existing frame controls plus a pinned source selected for vertical composition."""

import json
from pathlib import Path

import shapely
from verify_facade_frames import sources as original


def sources():
    data = json.loads(
        Path(__file__).with_name("data").joinpath("clad-facades.json").read_text()
    )

    def row(saved):
        return saved | {"polygon": shapely.from_wkb(saved["footprint_wkb"])}

    return original() + [
        (row(c["source"]), [row(r) for r in c["neighbors"]]) for c in data["cases"]
    ]
