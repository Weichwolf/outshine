"""Actual architecture and transport volumes on pinned whole Wiener source inputs."""

import argparse
import json
import resource
import time
from collections.abc import Mapping
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_consumer_buildings import buildings
from stair_consumer_inputs import inputs
from stair_occupancy_handoff import OccupancyQuery
from stair_terrain_experiment import frozen


def json_value(value):
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, np.generic):
        return value.item()
    if isinstance(value, Mapping):
        return dict(value)
    raise TypeError("unsupported consumer report object: " + type(value).__name__)


def save(output, name, value):
    path = output / name
    path.write_text(
        json.dumps(value, default=json_value, indent=2, allow_nan=False) + "\n"
    )
    return digest(path)


def save_meshes(output, name, meshes):
    arrays = {
        f"{i}_{key}": value
        for i, mesh in enumerate(meshes)
        for key, value in (("vertices", mesh.vertices), ("faces", mesh.triangles))
    }
    np.savez_compressed(output / name, **arrays)
    return digest(output / name)


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private consumer products require an explicit /tmp path")
    args.output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    data = inputs(args.products)
    loaded = time.perf_counter()
    before, after, occupied, ledger, context = buildings(
        data, OccupancyQuery(data["reservations"])
    )
    generated = time.perf_counter()
    artifacts = {
        "buildings-before.npz": save_meshes(
            args.output, "buildings-before.npz", before
        ),
        "buildings-after.npz": save_meshes(args.output, "buildings-after.npz", after),
        "building-ledger.json": save(args.output, "building-ledger.json", ledger),
        "context.json": save(args.output, "context.json", context),
    }
    result = {
        "commit": commit,
        "products": str(args.products.resolve()),
        "products_report_sha256": digest(args.products / "comparison.json"),
        "frame": data["frame"].identifier,
        "source_ledger_entries": len(ledger),
        "valid_polygon_parts": sum("id" in r for r in ledger),
        "invalid_original_features": sum("id" not in r for r in ledger),
        "prepared_parts": sum(r.get("state") == "prepared" for r in ledger),
        "deferred_parts": sum(r.get("state") != "prepared" for r in ledger),
        "actual_building_bodies": len(after),
        "building_reservations": len(occupied),
        "adjusted_source_parts": sum(
            r.get("state") == "prepared"
            and r["result"]["removed_footprint_area_m2"] > 1e-8
            for r in ledger
        ),
        "loading_seconds": loaded - start,
        "architecture_seconds": generated - loaded,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "artifacts": artifacts,
        "scope": "Actual existing Buildings producer consumes per-part height masks; single current terrain anchor, flat recipe then explicit Z translation. Not a whole-foundation certificate, world generator or native admission. Vegetation consumption follows separately.",
    }
    save(args.output, "comparison.json", result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    run(parser.parse_args())
