"""Fresh saved-volume reader checks every cap against original immutable XYZ bodies."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_local_replay import load_chain
from stair_module_inputs import reference_profiles
from stair_occupancy_experiment import compare
from stair_occupancy_geometry import AffineOccupancy
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import actual_sources, buildings, full_tile_sampler
from stair_terrain_experiment import frozen


def read(path, report):
    if (
        path.stat().st_size > 16 * 1024**2
        or digest(path) != report["volumes"]["sha256"]
    ):
        raise ValueError("bounded complete saved volume export required")
    data = json.loads(path.read_text())
    if not 1 <= len(data) <= 8192 or len({r["owner_id"] for r in data}) != len(data):
        raise ValueError("saved volume ownership missing or duplicated")
    rows = tuple(
        AffineOccupancy(
            r["owner_id"],
            shapely.from_wkb(r["geometry_wkb"]),
            r["purpose"],
            tuple(r["origin_xy_m"]),
            tuple(r["lower_plane_m"]),
            tuple(r["upper_plane_m"]),
        )
        for r in data
    )
    return rows, data


def cap_error(row, lower, upper, headroom):
    if not row.geometry.equals_exact(shapely.Polygon(upper[:, :2]), 0):
        raise ValueError("saved affine footprint differs from actual whole triangle")
    error = 0.0
    for actual, plane, delta in (
        (lower, row.lower_plane_m, 0),
        (upper, row.upper_plane_m, headroom),
    ):
        predicted = plane[0] + (actual[:, :2] - row.origin_xy_m) @ np.array(plane[1:])
        error = max(error, float(abs(predicted - actual[:, 2] - delta).max()))
    if error > 1e-8:
        raise ValueError("saved height plane differs from actual cap/headroom")
    return error


def check_caps(rows, data, assets, supports, contacts, manifest, report, sources):
    profiles, count, maximum = reference_profiles(), 0, 0.0
    for asset in assets:
        for face, (top, bottom) in enumerate(
            zip(asset.data["top_faces"], asset.data["bottom_faces"], strict=True)
        ):
            row = rows[count]
            ids = [
                k
                for k, flag in zip(
                    asset.source_ids, asset.data["owners"][face], strict=True
                )
                if flag
            ]
            h = max(
                profiles[sources[k]["properties"].get("subclass") or "unclassified"][
                    "clearanceM"
                ]
                for k in ids
            )
            if (
                row.owner_id != f"{asset.identity}:face:{face}"
                or data[count]["source_ids"] != ids
                or row.purpose != "transport.body-and-headroom"
            ):
                raise ValueError("actual road facet/source ownership changed")
            maximum = max(
                maximum,
                cap_error(
                    row,
                    asset.data["body_vertices"][bottom],
                    asset.data["body_vertices"][top],
                    h,
                ),
            )
            count += 1
    support_path = Path(report["support_ownership_report"]["path"])
    if digest(support_path) != report["support_ownership_report"]["sha256"]:
        raise ValueError("original source/station support ledger changed")
    support_report = json.loads(support_path.read_text())
    selected = support_report["methods"][support_report["selected"]]
    support_owners = {
        s["product"]: s["source_id"] for s in selected["stations"] if "product" in s
    }
    if selected["mesh_sha256"] != manifest["support_sha256"]:
        raise ValueError("source/station support ledger names another actual body")
    for i, (low, high) in enumerate(contacts):
        for face, triangle in enumerate(low.triangles):
            if (
                rows[count].owner_id
                != f"{manifest['support_sha256']}:support:{i}:face:{face}"
                or data[count]["source_ids"] != [support_owners[i]]
                or rows[count].purpose != "transport.support"
            ):
                raise ValueError("actual support facet ownership changed")
            maximum = max(
                maximum,
                cap_error(
                    rows[count], low.vertices[triangle], high.vertices[triangle], 0
                ),
            )
            count += 1
    if count != len(rows) or len(supports) != report["actual_supports"]:
        raise ValueError("saved actual-body volume ledger incomplete")
    return count, maximum


def verify(args):
    commit = frozen()
    root = args.products
    if not root.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private occupancy verification must live under /tmp")
    report = json.loads((root / "comparison.json").read_text())
    rows, data = read(root / "volumes.json", report)
    if set(report["artifacts"]) != {
        "building-sources.json",
        "probes.json",
        "adjusted-footprints.json",
    }:
        raise ValueError("saved occupancy artifact ledger incomplete")
    for name, expected in report["artifacts"].items():
        if digest(root / name) != expected:
            raise ValueError("saved occupancy product changed: " + name)
    _view, assets, supports, contacts, manifest = load_chain(
        report["resident"], report["prior_modules"]
    )
    if (
        manifest["frame"],
        manifest["root_resident_sha256"],
        manifest["parent_reports"],
        manifest["source_ids"],
    ) != (
        report["frame"],
        report["root_resident_sha256"],
        report["parent_reports"],
        report["source_ids"],
    ):
        raise ValueError("saved affine frame/source/parent binding changed")
    context, sources, _sample, _layer = actual_sources()
    count, maximum = check_caps(
        rows, data, assets, supports, contacts, manifest, report, sources
    )
    originals, ledger, _receipt = buildings(
        context, full_tile_sampler(Path(report["dem"]["path"]), context)
    )
    if ledger != json.loads((root / "building-sources.json").read_text()):
        raise ValueError("original provider features/height placements changed")
    _changes, actual, _cost = compare(OccupancyQuery(rows), originals)
    saved = json.loads((root / "adjusted-footprints.json").read_text())
    if len(saved) != len(actual) or any(
        p.source_id != s["source_id"]
        or list(p.part) != s["part"]
        or p.geometry.wkb_hex != s["geometry_wkb"]
        or [p.heights.lower_m, p.heights.upper_m] != s["heights_m"]
        for p, s in zip(actual, saved, strict=True)
    ):
        raise ValueError("fresh saved-volume spatial handoff disagrees")
    receipt = {
        "reader_commit": commit,
        "producer_commit": report["commit"],
        "products_report_sha256": digest(root / "comparison.json"),
        "verified_actual_facets": count,
        "maximum_plane_error_m": maximum,
        "source_count": len(manifest["source_ids"]),
        "whole_supports": len(supports),
        "building_source_ledger_count": len(ledger),
        "valid_saved_occupancy": True,
    }
    output = root / "verification.json"
    if output.exists():
        raise ValueError("fresh saved occupancy verification receipt required")
    output.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    verify(parser.parse_args())
