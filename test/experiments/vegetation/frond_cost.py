"""Fresh-process cost of an identical complete scalar or batched frond request."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import numpy as np
from frond_surface import FrondGuide


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mesh", type=Path, required=True)
    parser.add_argument("--guide", type=Path, required=True)
    parser.add_argument("--method", choices=("batch", "scalar"), required=True)
    args = parser.parse_args()
    began = time.perf_counter()
    with np.load(args.mesh) as data:
        mesh = dict(data)
    guide = FrondGuide.from_recipe(json.loads(args.guide.read_text()))
    load_s = time.perf_counter() - began
    began = time.perf_counter()
    result = guide.deform(mesh, args.method)
    elapsed = time.perf_counter() - began
    print(
        json.dumps(
            {
                "elapsed_s": elapsed,
                "load_s": load_s,
                "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
                * 1024,
                "sha256": {
                    key: hashlib.sha256(value.tobytes()).hexdigest()
                    for key, value in result.items()
                },
            }
        )
    )


if __name__ == "__main__":
    main()
