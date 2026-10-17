"""Two existing large parapet controls and two pinned actual service programmes."""

import json
from pathlib import Path

import shapely
from clad_facade_sources import sources as previous


def sources():
    data = json.loads(
        Path(__file__).with_name("data").joinpath("roof-crowns.json").read_text()
    )

    def row(saved):
        return saved | {"polygon": shapely.from_wkb(saved["footprint_wkb"])}

    old = previous()
    return [old[0], old[2]] + [
        (row(c["source"]), [row(n) for n in c["neighbors"]]) for c in data["cases"]
    ]
