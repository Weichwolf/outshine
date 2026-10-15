"""Explicit actual-Place source selection over the shared acquisition kernel."""

import argparse
import hashlib
import json
import re
from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict
from pathlib import Path

from outshine_ref.places import acquire, covering_tiles, receipt_bytes
from place_camera import selected_cameras

ROOT = Path(__file__).resolve().parents[3]


def providers(root):
    definitions = {}
    for kind, relative, suffix in (
        ("vector", "src/generators/osm/providers/PreferredVectorSource.h", "pbf"),
        ("elevation", "src/generators/ShippedProviders.cpp", "webp"),
    ):
        raw = (Path(root) / relative).read_bytes()
        match = re.search(
            rb'"(https://[^" ]+/\{z\}/\{x\}/\{y\}\.' + suffix.encode() + rb')"',
            raw,
        )
        if match is None:
            raise ValueError("preferred native provider definition missing: " + kind)
        definitions[kind] = {
            "template": match[1].decode(),
            "definition": relative,
            "definition_sha256": hashlib.sha256(raw).hexdigest(),
        }
    return definitions


def subjects(camera, definitions, *, vector_radius_m, terrain_radius_m):
    vectors = covering_tiles(
        camera.latitude_deg, camera.longitude_deg, vector_radius_m, 14
    )
    elevations = {
        tile
        for zoom, radius in (
            (8, terrain_radius_m),
            (11, 20000),
            (12, vector_radius_m + 2000),
        )
        for tile in covering_tiles(
            camera.latitude_deg, camera.longitude_deg, radius, zoom
        )
    }
    # Real MVT buffer counterexample from #205: keep neighbouring DEM parents.
    elevations.update(
        (11, (x // 8 + dx) % (1 << 11), y // 8 + dy)
        for _, x, y in vectors
        for dx in (-1, 0, 1)
        for dy in (-1, 0, 1)
        if 0 <= y // 8 + dy < (1 << 11)
    )
    if camera.samples_height:
        elevations.update(
            tile
            for zoom in range(12, 18)
            for tile in covering_tiles(
                camera.latitude_deg, camera.longitude_deg, 0, zoom
            )
        )
    return tuple(
        (
            kind,
            tile,
            definitions[kind]["template"].format(z=tile[0], x=tile[1], y=tile[2]),
        )
        for kind, tiles in (("vector", vectors), ("elevation", sorted(elevations)))
        for tile in tiles
    )


def run(
    root,
    output,
    *,
    fetch=False,
    reuse=None,
    vector_radius_m=3000,
    terrain_radius_m=120000,
):
    if not 1000 <= vector_radius_m <= 10000 or not 20000 <= terrain_radius_m <= 200000:
        raise ValueError("explicit bounded near vector and far terrain radii required")
    definitions, requests, places = providers(root), {}, []
    for camera in selected_cameras(root):
        selected = subjects(
            camera,
            definitions,
            vector_radius_m=vector_radius_m,
            terrain_radius_m=terrain_radius_m,
        )
        requests.update((row[2], row) for row in selected)
        places.append(
            {
                "camera": asdict(camera),
                "source_urls": [row[2] for row in selected],
                "initial_source_radius_m": vector_radius_m,
                "terrain_source_radius_m": terrain_radius_m,
            }
        )
    if len(requests) > 2048:
        raise ValueError("whole source request budget exceeded")
    shared = {}
    if reuse is not None:
        for row in json.loads(Path(reuse).read_text())["receipts"]:
            if row["url"] in requests and not row.get("missing"):
                receipt_bytes(row)
                shared[row["url"]] = row

    def load(subject):
        if subject[2] in shared:
            return shared[subject[2]] | {"origin": "explicit-shared-receipt"}
        return acquire(subject, Path(output) / "sources", fetch=fetch)

    with ThreadPoolExecutor(max_workers=4) as pool:
        receipts = list(pool.map(load, sorted(requests.values())))
    result = {
        "places": places,
        "receipts": receipts,
        "provider_definitions": definitions,
        "scope": "Actual sources, initial near vector window and coarse far DEM. Not complete 240 km world coverage, botanical recipes or native runtime admission.",
    }
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-receipts.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "requests": len(receipts),
                "bytes": sum(row.get("bytes", 0) for row in receipts),
                "missing": sum(bool(row.get("missing")) for row in receipts),
                "shared": len(shared),
                "output": str(output),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--reuse", type=Path)
    arguments = parser.parse_args()
    run(arguments.root, arguments.output, fetch=arguments.fetch, reuse=arguments.reuse)
