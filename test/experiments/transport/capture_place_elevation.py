"""Declare bounded shared DEM inputs for the unchanged Place camera stations."""

import argparse
import json
import subprocess
from pathlib import Path

from capture_height_targets import TEMPLATE
from outshine_ref.places import acquire, covering_tiles, read_camera
from transport_place_shots import PLACES, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fetch", action="store_true")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh external receipt file required")
    cases = {}
    for name, _source in PLACES:
        camera = read_camera(ROOT / "src/assets/places" / (name + ".scenario"))
        receipts = []
        if camera.samples_height:
            for zoom in (16, 15, 14):
                for tile in covering_tiles(
                    camera.latitude_deg, camera.longitude_deg, 0.0, zoom, max_tiles=4
                ):
                    url = TEMPLATE.format(z=tile[0], x=tile[1], y=tile[2])
                    receipts.append(
                        acquire(
                            ("elevation", tile, url),
                            args.cache,
                            fetch=args.fetch,
                            max_bytes=4 * 1024**2,
                            timeout_s=30,
                        )
                    )
        cases[name] = {"scenario_sha256": camera.scenario_sha256, "receipts": receipts}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(
            {
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
                ).strip(),
                "cases": cases,
                "scope": "Explicit offline camera DEM selection, z16 with declared z15/z14 parents; absolute cameras need no DEM. No source refresh or terrain-fit claim.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
