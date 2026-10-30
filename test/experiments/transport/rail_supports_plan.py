"""Bounded whole rail stations and preserved actual-contact/traffic admission."""

import time

import numpy as np
import shapely
from stair_support_plan import admit as admit_contacts
from stair_support_plan import station_choices

VARIANTS = {
    "fixed": ((0, 0),),
    "longitudinal": ((0, 0), (-2, 0), (2, 0), (-4, 0), (4, 0)),
}


def footprint(line, station, lateral=0):
    if (
        not np.isfinite([station, lateral]).all()
        or not 2 <= station <= line.length - 2
        or abs(lateral) > 1.2
    ):
        raise ValueError("Bounded whole rail support station/lateral position required")
    centre = np.asarray(line.interpolate(station).coords[0])
    a, b = (
        np.asarray(line.interpolate(station - 0.1).coords[0]),
        np.asarray(line.interpolate(station + 0.1).coords[0]),
    )
    axis = b - a
    length = np.linalg.norm(axis)
    if length <= 1e-7:
        raise ValueError("Rail station has no bounded local tangent")
    axis /= length
    normal = np.array([-axis[1], axis[0]])
    corners = (
        np.array([[-0.6, -0.3], [0.6, -0.3], [0.6, 0.3], [-0.6, 0.3]])
        @ np.array([axis, normal])
        + centre
        + lateral * normal
    )
    return shapely.Polygon(corners)


def spacing(rows, ledger):
    results = []
    for row in rows:
        length = shapely.LineString(row["xy"]).length
        selected = sorted(
            s["nominal_m"] + s["shift_m"]
            for s in ledger
            if s["source_id"] == row["source_id"] and "product" in s
        )
        gap = float(np.diff([0, *selected, length]).max())
        results.append(
            {
                "source_id": row["source_id"],
                "length_m": length,
                "supplied_xy_endpoints_m": np.asarray(row["xy"])[[0, -1]].tolist(),
                "supported_stations": len(selected),
                "maximum_gap_m": gap,
                "within_recipe_span": gap <= 24,
            }
        )
    return results


def admit_station(context, product, expected, previous, work):
    actual, physical = admit_contacts(
        product,
        context.lower_reference,
        context.upper_reference,
        context.exclusions,
        [[], []],
    )
    if (
        actual[0].symmetric_difference(expected).area > 1e-8
        or actual[0].hausdorff_distance(expected) > 1e-7
    ):
        raise ValueError(
            "Actual support differs from its whole source station footprint"
        )
    for candidate, pool in zip(actual, previous, strict=True):
        work["new_pier_pair_checks"] += len(pool)
        if pool and np.any(shapely.dwithin(candidate, pool, 0.2)):
            raise ValueError("new_pier_foreign_distance")
    return actual, physical


def plan(context, rows, *, method="longitudinal", seconds=8, precheck=True):
    if (
        method not in VARIANTS
        or not np.isfinite(seconds)
        or not 0 < seconds <= 8
        or type(precheck) is not bool
    ):
        raise ValueError("Bounded known rail support planning method required")
    if not 1 <= len(rows) <= 8 or any(
        r.get("original_properties", {}).get("brunnel") != "bridge"
        or r.get("original_properties", {}).get("subclass") not in ("rail", "subway")
        for r in rows
    ):
        raise ValueError("Whole supplied elevated rail roles required")
    candidates, short, deferred = station_choices(
        [{**r, "original": {"bridge": True}} for r in rows], selection="uniform"
    )
    began, cpu = time.perf_counter(), time.process_time()
    products, ledger, shapes = [], [], [[], []]
    work = {
        "attempts": 0,
        "prechecks": 0,
        "builder_calls": 0,
        "builder_successes": 0,
        "contact_pair_checks": 0,
        "new_pier_pair_checks": 0,
        "body_faces": 0,
    }
    for owner, line, nominal in candidates:
        row = {
            "source_id": rows[owner]["source_id"],
            "nominal_m": nominal,
            "trials": [],
        }
        for shift, lateral in VARIANTS[method]:
            if time.perf_counter() - began >= seconds or work["attempts"] >= 640:
                row["reason"] = "support_time_or_trial_budget"
                break
            work["attempts"] += 1
            trial = {"shift_m": shift, "lateral_m": lateral, "builder_called": False}
            try:
                shape = footprint(line, nominal + shift, lateral)
                if precheck:
                    work["prechecks"] += 1
                    if context.exclusions[0].conflicts(shape):
                        raise ValueError("old_traffic_or_support_exclusion")
                work["builder_calls"] += 1
                trial["builder_called"] = True
                product, note = context.builder.build(shape, minimum_height_m=0.1001)
                if product is None:
                    raise ValueError(note["reason"])
                work["builder_successes"] += 1
                actual, physical = admit_station(context, product, shape, shapes, work)
                for pool, value in zip(shapes, actual, strict=True):
                    pool.append(value)
                row.update(
                    product=len(products),
                    shift_m=shift,
                    lateral_m=lateral,
                    geometry=note,
                    physical=physical,
                    reason="",
                )
                products.append(product)
                work["body_faces"] += len(product.body.triangles)
                work["contact_pair_checks"] += sum(
                    physical[k]["contact_pair_checks"]
                    for k in ("lower_contact", "upper_contact")
                )
            except (ValueError, shapely.errors.GEOSException) as failure:
                row["reason"] = str(failure)
            trial["reason"] = row["reason"]
            row["trials"].append(trial)
            if "product" in row:
                break
        ledger.append(row)
    return products, {
        "method": method,
        "precheck": precheck,
        "accepted": len(products),
        "stations": ledger,
        "short_sources": short,
        "deferred_stations": deferred,
        "work": work,
        "source_spacing": spacing(rows, ledger),
        "seconds": time.perf_counter() - began,
        "cpu_seconds": time.process_time() - cpu,
        "scope": "Whole procedural support station candidates with explicit local omissions. No structural/foundation/complete rail/routing/source/world/device admission.",
    }
