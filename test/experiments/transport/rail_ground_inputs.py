"""Pinned original rail components and the explicitly required DEM neighbor."""

import gzip
import hashlib
import json
from pathlib import Path

import numpy as np
from ground_cyclic_inputs import prepare
from outshine_ref.places.height import Elevation

FIXTURE = Path(__file__).parent / "fixtures/rail-ground"


def load():
    manifest = json.loads((FIXTURE / "manifest.json").read_text())
    if manifest["schema"] != "outshine/constructive-rail-ground-inputs/1":
        raise ValueError("Known explicit constructive rail inputs required")
    source_manifest = manifest["original_source_manifest"]
    path = FIXTURE / source_manifest["path"]
    if hashlib.sha256(path.read_bytes()).hexdigest() != source_manifest["sha256"]:
        raise ValueError("Original rail source capture changed")
    index, cases, _old_terrain, budget, metadata = prepare(
        minimum_cycles=3, maximum_cycles=8, maximum_spans=32
    )
    receipts = []
    for key in ("existing_elevation", "additional_elevation"):
        row = dict(manifest[key])
        row["path"] = str((FIXTURE / row["path"]).resolve())
        receipts.append(row)
    sampler = Elevation(receipts)

    def terrain(geographic):
        geographic = np.asarray(geographic)
        return sampler.sample(geographic[:, 1], geographic[:, 0])

    with gzip.open(path.parent / "source.mvt.gz", "rb") as stream:
        raw_source = stream.read(4 * 1024 * 1024 + 1)
    if len(raw_source) > 4 * 1024 * 1024:
        raise ValueError("Bounded original rail source required")
    rail_cases = [
        case
        for case in cases
        if any(index[1][e].maximum_gradient is None for e in case["selected"])
    ]
    return index, rail_cases, terrain, budget, metadata, raw_source, manifest
