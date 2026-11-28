"""Print source digests for a vegetation capture through the normal scene builder."""

import argparse
import json
from pathlib import Path

from outshine.world.scene import ReferenceScene

from ..vienna.catalog import available_views
from .catalog import SOURCE_CACHE


def collect_pins(config, places):
    pins = {}
    for place in places:
        for receipt in ReferenceScene(place, config).source_receipts:
            url = receipt["url"]
            digest = None if receipt.get("missing") else receipt["sha256"]
            if pins.setdefault(url, digest) != digest:
                raise ValueError(f"Source changed during capture: {url}")
    return pins


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--cache", type=Path, default=SOURCE_CACHE)
    args = parser.parse_args()
    config = json.loads(args.capture.read_text())
    if config["source_snapshot"] == "latest":
        parser.error("capture requires a dated OpenFreeMap snapshot")
    config.pop("source_pins", None)
    config["source_cache"] = str(args.cache.resolve())
    pins = collect_pins(config, available_views(config))
    print(json.dumps(pins, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
