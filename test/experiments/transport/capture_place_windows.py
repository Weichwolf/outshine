"""Capture immutable MVT windows selected by the fixed Place viewing directions."""

import argparse
import json
import math
import shutil
import subprocess
from pathlib import Path

import numpy as np
import shapely
from capture_height_targets import world_coordinates
from capture_transport_tile import capture
from outshine_ref.places import acquire as acquire_source
from outshine_ref.places import read_camera
from transport_place_inputs import mercator
from transport_place_shots import PLACES, ROOT, flat_model


def camera_tiles(name, origin, *, distance_m=1600.0):
    camera = read_camera(ROOT / "src/assets/places" / (name + ".scenario"))
    xy = (
        mercator(camera.latitude_deg, camera.longitude_deg) - mercator(*origin)
    ) * math.cos(math.radians(origin[0]))
    half = math.atan(
        math.tan(math.radians(camera.vertical_fov_deg) / 2)
        * camera.native_profile[0]
        / camera.native_profile[1]
    )
    angles = math.radians(camera.bearing_deg) + np.array([-half, 0.0, half])
    points = np.vstack((xy, xy + distance_m * np.c_[np.sin(angles), np.cos(angles)]))
    tiles = np.floor(world_coordinates(origin, points) * (1 << 14)).astype(int)
    lo, hi = tiles.min(axis=0), tiles.max(axis=0)
    if np.any(hi - lo > 2):
        raise ValueError("bounded local Place source window required")
    return {
        (14, x, y) for x in range(lo[0], hi[0] + 1) for y in range(lo[1], hi[1] + 1)
    }


def acquire(url, cache):
    parts = url.rsplit("/", 3)
    tile = (int(parts[1]), int(parts[2]), int(parts[3].removesuffix(".pbf")))
    return acquire_source(
        ("vector", tile, url), cache, fetch=True, max_bytes=8 * 1024**2, timeout_s=30
    )


def reserve_axes(merged, reserved, reason):
    merged["roads"], merged["reserved_sources"] = [], []
    for row in merged["original_axes"]:
        if shapely.LineString(row["coordinates"]).distance(reserved) <= 64.0:
            merged["reserved_sources"].append(
                {"id": row["source_id"], "reason": reason}
            )
        else:
            merged["roads"].append(row)


def case(name, source, args, reservations):
    references = json.loads(
        (ROOT / "test/experiments/transport/reference/place-sources.json").read_text()
    )
    previous = next(row for row in references["cases"] if row["source"] == source)
    origin = previous["origin_lat_lon"]
    selected = camera_tiles(name, origin)
    if name != "Wien":
        selected.add(tuple(previous["receipt"]["tile"]))
    cache = args.cache
    prefix = previous["receipt"]["url"].rsplit("/", 3)[0]
    merged = {
        "origin_lat_lon": origin,
        "original_features": [],
        "deferred_sources": [],
        "roads": [],
        "receipts": [],
    }
    stage = args.output / "tile-capture"
    stage.mkdir(exist_ok=True)
    for tile in sorted(selected):
        label = source + "-" + "-".join(map(str, tile))
        receipt = acquire(prefix + "/" + "/".join(map(str, tile)) + ".pbf", cache)
        if (
            list(tile) == previous["receipt"]["tile"]
            and receipt["sha256"] != previous["receipt"]["sha256"]
        ):
            raise ValueError("immutable legacy tile hash changed")
        receipt.update(tile=list(tile), sites=[label])
        inventory = stage / "inventory.json"
        inventory.write_text(
            json.dumps({"sites": [[label, *origin]], "receipts": [receipt]})
        )
        capture(label, inventory, cache, stage)
        data = json.loads((stage / (label + "-input.json")).read_text())
        for key in ("original_features", "deferred_sources", "roads"):
            merged[key].extend(data[key])
        merged["receipts"].append(data["receipt"])
    merged["original_axes"] = list(merged["roads"])
    merged["reserved_sources"] = []
    if name in reservations:
        reserved, reason = reservations[name]
        reserve_axes(merged, reserved, reason)
    merged["camera_window"] = {
        "place": name,
        "distance_m": 1600.0,
        "whole_tiles": [list(t) for t in sorted(selected)],
        "retained_legacy_tile": name != "Wien",
        "immutable_provider_prefix": prefix,
        "reserved_ground_guard_m": 64.0 if name in reservations else 0.0,
    }
    path = args.output / "inputs" / (source + "-input.json")
    path.write_text(json.dumps(merged, separators=(",", ":")) + "\n")
    print(
        json.dumps(
            {
                "place": name,
                "tiles": len(selected),
                "original_axes": len(merged["original_axes"]),
                "planned_input_axes": len(merged["roads"]),
                "reserved": len(merged["reserved_sources"]),
            }
        ),
        flush=True,
    )
    return [source, *origin]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--port-plan", type=Path, required=True)
    parser.add_argument("--retain-flat-release", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("fresh empty external output required")
    (args.output / "inputs").mkdir(parents=True)
    with np.load(args.port_plan / "plan.npz") as plan:
        xy = plan["xy"]
        reserved = shapely.box(*xy.min(axis=0), *xy.max(axis=0))
    reservations = {"Wien": (reserved, "existing_verified_height_plan_reservation")}
    if args.retain_flat_release is not None:
        pins = json.loads((args.retain_flat_release / "sha256.json").read_text())
        mesh, _, _ = flat_model("CentralPark", args.retain_flat_release, pins, 0.0)
        xy = mesh.vertices[:, :2]
        reservations["CentralPark"] = (
            shapely.box(*xy.min(axis=0), *xy.max(axis=0)),
            "existing_verified_flat_plan_reservation",
        )
    sites = [case(name, source, args, reservations) for name, source in PLACES]
    (args.output / "inventory.json").write_text(
        json.dumps({"sites": sites}, indent=2) + "\n"
    )
    shutil.rmtree(args.output / "tile-capture")
    (args.output / "capture.json").write_text(
        json.dumps(
            {
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
                ).strip(),
                "sites": sites,
                "original_height_plan_reserved_xy_bounds": list(reserved.bounds),
                "retained_flat_cases": ["CentralPark"]
                if args.retain_flat_release
                else [],
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
