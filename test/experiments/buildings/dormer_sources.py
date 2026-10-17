"""Three real concave/courtyard urban roofs from the existing pinned provider input."""

import json
from pathlib import Path

import shapely
from verify_urban_roofs import sources as original


def sources():
    return [(row | {"layer": "building"}, []) for row in original()]


def counterexamples():
    data = json.loads(
        Path(__file__)
        .with_name("data")
        .joinpath("dormer-counterexamples.json")
        .read_text()
    )
    return [
        row | {"polygon": shapely.from_wkb(row["footprint_wkb"])}
        for row in data["cases"]
    ]
