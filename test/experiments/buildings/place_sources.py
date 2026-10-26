"""Explicit preferred-provider acquisition for the four fixed Python Place views."""

import argparse
import concurrent.futures
import json
from dataclasses import asdict
from pathlib import Path

from outshine_ref.places import acquire as acquire_source
from outshine_ref.places import covering_tiles as tiles
from place_reference import PLACES, flat
from samples import SOURCES
from special_sources import endpoint


def acquire(subject, fetch):
    return acquire_source(subject, SOURCES.parent, fetch=fetch)


def run(output, fetch, radius_m):
    if not 1000 <= radius_m <= 20000:
        raise ValueError("Explicit Place source radius requires 1000..20000 metres")
    vector, vector_path, vector_sha = endpoint()
    subjects, places = {}, []
    for name in PLACES:
        reference = flat(name)
        selected = []
        vector_tiles = tiles(
            reference.latitude_deg, reference.longitude_deg, radius_m, 14
        )
        for tile in vector_tiles:
            url = vector.format(z=tile[0], x=tile[1], y=tile[2])
            subjects[url] = "vector", tile, url
            selected.append(url)
        places.append(
            {
                "camera": asdict(reference),
                "source_urls": selected,
                "initial_source_radius_m": radius_m,
            }
        )
    print(
        "Acquiring",
        len(subjects),
        "source tiles for",
        len(places),
        "fixed Place cameras",
        flush=True,
    )
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        receipts = list(
            pool.map(lambda subject: acquire(subject, fetch), subjects.values())
        )
    result = {
        "places": places,
        "receipts": sorted(receipts, key=lambda row: (row["kind"], row["tile"])),
        "provider_definitions": [
            {"kind": "vector", "path": vector_path, "sha256": vector_sha},
        ],
        "ground_mode": "flat-no-dem",
        "scope": "Only preferred OSM vector sources for a bounded flat building study, no DEM acquisition. Place position, bearing and lens retained; fixed flat camera heights replace geographic altitudes. Not complete native world coverage. Raw provider bytes remain in the source cache.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        "Acquired",
        sum(row.get("bytes", 0) for row in receipts),
        "vector source bytes; no DEM requested",
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--radius-m", type=float, default=3000.0)
    args = parser.parse_args()
    run(args.output, args.fetch, args.radius_m)
