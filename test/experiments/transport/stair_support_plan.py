"""Bounded pier stations on actual stair-network bridge undersides and terrain."""

import json
import time

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from stair_cycle_inputs import load
from stair_support_audit import Reference, body_caps, domain, quantized
from stair_support_traffic import RegionMask, TrafficIndex


def inputs(root, fixture):
    report = json.loads((root / "comparison.json").read_text())
    replay = json.loads((root / "verification.json").read_text())
    if not replay["valid_local_candidate"] or replay[
        "products_report_sha256"
    ] != digest(root / "comparison.json"):
        raise ValueError("verified whole source and terrain products required")
    if report["fixture_sha256"] != digest(fixture):
        raise ValueError("support source fixture changed")
    paths = dict(report["artifacts"])
    paths["after/body.npz"] = report["after"]["body_sha256"]
    paths["after-display.npz"] = report["display"]["after_display_sha256"]
    paths["source-catalog.json"] = report["source_catalog_sha256"]
    if any(digest(root / name) != expected for name, expected in paths.items()):
        raise ValueError("support baseline artifact changed")
    _case, rows, _nodes, _stairs, context, _sample = load(fixture)
    if report["candidate_source_ids"] != [r["source_id"] for r in rows]:
        raise ValueError("support baseline source ownership changed")
    road = dict(np.load(root / "after/body.npz"))
    terrain = dict(np.load(root / "membrane-body.npz"))
    lower = Mesh(terrain["vertices"], terrain["top_faces"])
    return rows, road, lower, context, paths


def station_choices(rows, *, selection="all"):
    if selection not in ("all", "uniform") or not 1 <= len(rows) <= 64:
        raise ValueError("known bounded support station selection required")
    result, short = [], []
    for owner, row in enumerate(rows):
        if not row["original"]["bridge"]:
            continue
        line = shapely.LineString(row["xy"])
        if not 0 < line.length <= 10000:
            raise ValueError("bounded positive bridge source required")
        values = np.arange(6, line.length - 2, 12)
        if not len(values):
            short.append(
                {"source_id": row["source_id"], "reason": "no_interior_station"}
            )
        result.extend((owner, line, float(s)) for s in values)
        if len(result) > 128 and selection == "all":
            raise ValueError("stair support station budget")
    deferred = []
    if len(result) > 128:
        groups = {}
        for i, (owner, _line, _station) in enumerate(result):
            groups.setdefault(owner, []).append(i)
        keep = {i for values in groups.values() for i in (values[0], values[-1])}
        pool = [i for i in range(len(result)) if i not in keep]
        slots = 128 - len(keep)
        if slots == 1:
            keep.add(pool[len(pool) // 2])
        elif slots > 1:
            keep.update(
                pool[(k * (len(pool) - 1) + (slots - 1) // 2) // (slots - 1)]
                for k in range(slots)
            )
        deferred = [
            {
                "source_id": rows[owner]["source_id"],
                "nominal_m": station,
                "reason": "support_station_budget",
            }
            for i, (owner, _line, station) in enumerate(result)
            if i not in keep
        ]
        result = [item for i, item in enumerate(result) if i in keep]
    return result, short, deferred


def stations(rows):
    result, short, _deferred = station_choices(rows)
    return result, short


def footprint(line, station):
    if not 2 <= station <= line.length - 2:
        raise ValueError("support station outside finite source")
    centre = np.array(line.interpolate(station).coords)[0]
    tangent = (
        np.array(line.interpolate(station + 0.1).coords)[0]
        - np.array(line.interpolate(station - 0.1).coords)[0]
    )
    length = np.linalg.norm(tangent)
    if length <= 1e-7:
        raise ValueError("support station has no local tangent")
    tangent /= length
    normal = np.array([-tangent[1], tangent[0]])
    corners = np.array([[-0.4, -0.225], [0.4, -0.225], [0.4, 0.225], [-0.4, 0.225]])
    return shapely.Polygon(corners @ np.array([tangent, normal]) + centre)


def source_surfaces(road, owners, *, preparation="shared"):
    if preparation not in ("shared", "per_source"):
        raise ValueError("known support preparation required")
    upper_surfaces, exclusions = {}, {}
    if not owners:
        return upper_surfaces, exclusions
    if len(road["top_faces"]) > 65536 or len(road["top_faces"]) != len(
        road["face_sources"]
    ):
        raise ValueError("bounded matching traffic faces and source masks required")
    rounded = road["body_vertices"].astype("f4").astype(float)
    indices = (
        [
            TrafficIndex(
                shapely.polygons(xyz[road["top_faces"], :2]), road["face_sources"]
            )
            for xyz in (road["body_vertices"], rounded)
        ]
        if preparation == "shared"
        else None
    )
    for owner in sorted(owners):
        upper = Mesh(
            road["body_vertices"],
            road["bottom_faces"][(road["face_sources"] & (1 << owner)) != 0],
        )
        upper_surfaces[owner] = upper
        if indices is not None:
            exclusions[owner] = [index.for_source(owner) for index in indices]
        else:
            other = (road["face_sources"] & ~(1 << owner)) != 0
            exclusions[owner] = [
                RegionMask(
                    shapely.union_all(
                        shapely.polygons(xyz[road["top_faces"][other], :2])
                    )
                )
                for xyz in (road["body_vertices"], rounded)
            ]
    return upper_surfaces, exclusions


def context(road, lower, owners, *, preparation="shared"):
    upper_surfaces, exclusions = source_surfaces(road, owners, preparation=preparation)
    if preparation == "shared" and owners:
        bits = sum(1 << owner for owner in owners)
        upper = Mesh(
            road["body_vertices"],
            road["bottom_faces"][(road["face_sources"] & bits) != 0],
        )
        shared = SupportVolumeBuilder(lower, upper)
        builders = {owner: shared for owner in owners}
    else:
        builders = {
            owner: SupportVolumeBuilder(lower, upper)
            for owner, upper in upper_surfaces.items()
        }
    references = {owner: Reference(upper) for owner, upper in upper_surfaces.items()}
    unique = {id(builder): builder for builder in builders.values()}
    traffic_faces = (
        2 * len(road["top_faces"])
        if preparation == "shared" and owners
        else 2
        * sum(
            int(np.count_nonzero((road["face_sources"] & ~(1 << owner)) != 0))
            for owner in owners
        )
    )
    work = {
        "mode": preparation,
        "construction_builders": len(unique),
        "construction_lower_faces": sum(
            len(b.lower.triangles) for b in unique.values()
        ),
        "construction_upper_faces": sum(
            len(b.upper.triangles) for b in unique.values()
        ),
        "traffic_input_triangles": traffic_faces,
        "traffic_indices": 2 if preparation == "shared" and owners else 0,
        "traffic_union_calls": 2 * len(owners) if preparation == "per_source" else 0,
        "traffic_triangle_xy_payload_bytes": traffic_faces
        * 3
        * 2
        * road["body_vertices"].dtype.itemsize,
        "owned_traffic_mask_bytes": 2 * len(road["face_sources"]) * 8
        if preparation == "shared" and owners
        else 0,
    }
    return builders, Reference(lower), references, exclusions, work


def admit(product, lower, upper, excluded, previous):
    note = body_caps(product)
    shapes = [domain(product.lower_contact), domain(quantized(product.lower_contact))]
    if any(
        region.conflicts(shape) for shape, region in zip(shapes, excluded, strict=True)
    ):
        raise ValueError("actual support conflicts with another selected source")
    for shape, old in zip(shapes, previous, strict=True):
        if old and shapely.intersects(shape, old).any():
            raise ValueError("actual support conflicts with an earlier pier")
    note["lower_contact"] = lower.check(product.lower_contact)
    note["upper_contact"] = upper.check(product.upper_contact)
    return shapes, note


def plan(
    rows,
    road,
    lower,
    *,
    method,
    seconds=8,
    preparation="shared",
    station_selection="all",
):
    if method not in ("fixed", "shifted") or not 0 < seconds <= 8:
        raise ValueError("bounded known stair support method required")
    began = time.perf_counter()
    candidates, short, deferred = station_choices(rows, selection=station_selection)
    builders, low_ref, uppers, exclusions, preparation_work = context(
        road, lower, {c[0] for c in candidates}, preparation=preparation
    )
    preparation_elapsed = time.perf_counter() - began
    products, ledger, shapes = [], [], [[], []]
    shifts = (0,) if method == "fixed" else (0, -2, 2, -4, 4)
    work = {"trials": 0, "contact_pair_checks": 0, "body_faces": 0}
    for owner, line, nominal in candidates:
        row = {
            "source_id": rows[owner]["source_id"],
            "nominal_m": nominal,
            "trials": [],
        }
        for shift in shifts:
            if time.perf_counter() - began >= seconds or work["trials"] >= 640:
                row["reason"] = "support_time_or_trial_budget"
                break
            work["trials"] += 1
            try:
                shape = footprint(line, nominal + shift)
                if exclusions[owner][0].conflicts(shape):
                    raise ValueError("other_selected_source_exclusion")
                product, geometry = builders[owner].build(
                    shape, minimum_height_m=0.1001
                )
                if product is None:
                    raise ValueError(geometry["reason"])
                actual, checked = admit(
                    product, low_ref, uppers[owner], exclusions[owner], shapes
                )
                for pool, value in zip(shapes, actual, strict=True):
                    pool.append(value)
                row.update(
                    product=len(products),
                    shift_m=shift,
                    geometry=geometry,
                    physical=checked,
                )
                products.append(product)
                work["body_faces"] += len(product.body.triangles)
                work["contact_pair_checks"] += sum(
                    checked[key]["contact_pair_checks"]
                    for key in ("lower_contact", "upper_contact")
                )
                row["reason"] = ""
            except (ValueError, shapely.errors.GEOSException) as failure:
                row["reason"] = str(failure)
            row["trials"].append({"shift_m": shift, "reason": row["reason"]})
            if "product" in row:
                break
        ledger.append(row)
    selection_note = (
        {}
        if station_selection == "all"
        else {"station_selection": station_selection, "deferred_stations": deferred}
    )
    return products, {
        **selection_note,
        "method": method,
        "stations": ledger,
        "short_sources": short,
        "work": work,
        "accepted": len(products),
        "preparation_seconds": preparation_elapsed,
        "preparation_work": preparation_work,
        "seconds": time.perf_counter() - began,
    }
