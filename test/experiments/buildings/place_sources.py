"""Explicit preferred-provider acquisition for the four fixed Python Place views."""

import argparse
import concurrent.futures
import hashlib
import json
import re
import subprocess
from dataclasses import asdict
from pathlib import Path

from place_reference import PLACES, ROOT, read
from samples import SOURCES
from special_sources import endpoint, tiles


def elevation_endpoint():
    source = ROOT / "src/generators/ShippedProviders.cpp"
    value = re.search(r'"(https://[^" ]+/\{z\}/\{x\}/\{y\}\.webp)"', source.read_text())
    if value is None:
        raise ValueError("Preferred elevation endpoint missing")
    return (
        value[1],
        str(source.relative_to(ROOT)),
        hashlib.sha256(source.read_bytes()).hexdigest(),
    )


def acquire(subject, fetch):
    kind, tile, url = subject
    folder = SOURCES if kind == "vector" else SOURCES.parent / "mapterhorn"
    path = folder / (
        hashlib.sha256(url.encode()).hexdigest()
        + (".pbf" if kind == "vector" else ".webp")
    )
    if path.is_file():
        data, origin = path.read_bytes(), "existing-source-cache"
    elif fetch:
        encoded = subprocess.check_output(
            [
                "curl",
                "--silent",
                "--show-error",
                "--location",
                "--max-time",
                "60",
                "--max-filesize",
                str(16 * 1024 * 1024),
                "--write-out",
                "\n%{http_code}",
                url,
            ]
        )
        data, status = encoded[:-4], encoded[-3:].decode("ascii")
        if status == "404" and kind == "elevation":
            return {
                "kind": kind,
                "tile": list(tile),
                "url": url,
                "missing": True,
                "reason": "Preferred elevation provider returns HTTP 404",
            }
        if status != "200":
            raise ValueError("Preferred provider HTTP " + status + ": " + url)
        if not data:
            raise ValueError("Preferred provider returned an empty source")
        folder.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_bytes(data)
        temporary.replace(path)
        origin = "network"
    else:
        raise FileNotFoundError(str(path) + "; acquisition requires --fetch")
    return {
        "kind": kind,
        "tile": list(tile),
        "url": url,
        "path": str(path),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "origin": origin,
    }


def run(output, fetch, radius_m):
    if not 1000 <= radius_m <= 20000:
        raise ValueError("Explicit Place source radius requires 1000..20000 metres")
    vector, vector_path, vector_sha = endpoint()
    elevation, elevation_path, elevation_sha = elevation_endpoint()
    subjects, places = {}, []
    for name in PLACES:
        reference = read(name)
        selected = []
        vector_tiles = tiles(
            reference.latitude_deg, reference.longitude_deg, radius_m, 14
        )
        dem_tiles = sorted(
            {
                (11, x // 8 + dx, y // 8 + dy)
                for _zoom, x, y in vector_tiles
                for dx in (-1, 0, 1)
                for dy in (-1, 0, 1)
            }
        )
        for kind, template, selected_tiles in (
            ("vector", vector, vector_tiles),
            ("elevation", elevation, dem_tiles),
        ):
            for tile in selected_tiles:
                url = template.format(z=tile[0], x=tile[1], y=tile[2])
                subjects[url] = kind, tile, url
                selected.append(url)
        if reference.samples_height:
            for zoom in range(12, 18):
                tile = tiles(
                    reference.latitude_deg, reference.longitude_deg, 0.0, zoom
                )[0]
                url = elevation.format(z=tile[0], x=tile[1], y=tile[2])
                subjects[url] = "elevation", tile, url
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
            {"kind": "elevation", "path": elevation_path, "sha256": elevation_sha},
        ],
        "scope": "Initial explicitly bounded source acquisition, not complete 240 km native Place coverage. Missing distant sources/world classes remain unproven; camera and lens are not moved to fit this window. Raw provider bytes remain in the source cache.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        "Acquired",
        sum(row.get("bytes", 0) for row in receipts),
        "source bytes;",
        sum(row.get("missing", False) for row in receipts),
        "explicit missing elevation tiles",
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--radius-m", type=float, default=3000.0)
    args = parser.parse_args()
    run(args.output, args.fetch, args.radius_m)
