"""Portable captured inputs only; no generation or pin updates during normal runs."""

import gzip
import hashlib
import json
from pathlib import Path

import numpy as np

REFERENCE = Path(__file__).resolve().parent / "reference/port-planes"


def load_reference():
    metadata = json.loads((REFERENCE / "metadata.json").read_text())
    for name, pin in metadata["files"].items():
        path = REFERENCE / name
        if (
            path.stat().st_size != pin["bytes"]
            or hashlib.sha256(path.read_bytes()).hexdigest() != pin["sha256"]
        ):
            raise ValueError("captured port-plane input bytes changed: " + name)
    with np.load(REFERENCE / "Vienna.npz") as source:
        arrays = {key: source[key] for key in source.files}
    product = {
        key.removeprefix("flat_"): value
        for key, value in arrays.items()
        if key.startswith("flat_")
    }
    count = int(arrays["road_face_count"])
    mapping = arrays["road_vertex_map"]
    if not np.array_equal(arrays["ground_xy"][mapping], product["vertices"]):
        raise ValueError("captured joint/road vertices disagree")
    if not np.array_equal(arrays["ground_faces"][:count], mapping[product["indices"]]):
        raise ValueError("captured joint/road triangles disagree")
    if not np.array_equal(arrays["road_targets"], arrays["ground_targets"][mapping]):
        raise ValueError("captured metric height priors disagree")
    if not np.array_equal(arrays["road_frames"], arrays["ground_frames"][:count]):
        raise ValueError("captured nominal road grade frames disagree")
    sources = json.loads(gzip.decompress((REFERENCE / "sources.json.gz").read_bytes()))
    return product, arrays, metadata, sources
