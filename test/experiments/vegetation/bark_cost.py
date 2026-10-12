"""Measure one complete material request in a fresh process, including peak RSS."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import numpy as np
from bark_coordinates import material_coordinates
from plant_experiment import sapling


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mesh", type=Path, required=True)
    parser.add_argument("--method", choices=("dense", "indexed"), required=True)
    args = parser.parse_args()
    began = time.perf_counter()
    with np.load(args.mesh) as data:
        vertices = data["vertices"]
    load_s = time.perf_counter() - began
    plant = sapling()
    began = time.perf_counter()
    fields, work = material_coordinates(plant, vertices, args.method)
    elapsed = time.perf_counter() - began
    assert not plant._leaves
    print(
        json.dumps(
            {
                "elapsed_s": elapsed,
                "input_load_s": load_s,
                "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
                * 1024,
                "work": work,
                "sha256": {
                    key: hashlib.sha256(value.tobytes()).hexdigest()
                    for key, value in fields.items()
                },
            }
        )
    )


if __name__ == "__main__":
    main()
