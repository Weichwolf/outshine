"""Bounded procedural support stations; traffic/portal conflicts omit only local piers."""

import time

import numpy as np
import shapely
from outshine_ref.transport.supports import SupportVolumeBuilder


def stations(sources, spacing=24.0):
    if len(sources) > 256 or not np.isfinite(spacing) or spacing < 8:
        raise ValueError("bounded support sources and station spacing required")
    rows = []
    for source in sources:
        xy = np.asarray(source["xy"], float)
        if not 2 <= len(xy) <= 4096 or not np.isfinite(xy).all():
            raise ValueError("bounded finite original bridge axis required")
        line = shapely.LineString(xy)
        if not 0 < line.length <= 10000:
            raise ValueError("bounded positive bridge axis length required")
        values = np.arange(spacing / 2, line.length - 2, spacing)
        if len(rows) + len(values) > 1024:
            raise ValueError("support station budget")
        for station in values:
            rows.append(
                {
                    "source_id": source["source_id"],
                    "station_m": float(station),
                    "line": line,
                }
            )
    return rows


def footprint_at(line, station):
    if not 2 <= station <= line.length - 2:
        raise ValueError("support station outside finite source")
    xy = np.array(line.interpolate(station).coords)[0]
    tangent = (
        np.array(line.interpolate(station + 0.1).coords)[0]
        - np.array(line.interpolate(station - 0.1).coords)[0]
    )
    length = np.linalg.norm(tangent)
    if length <= 1e-7:
        raise ValueError("support source turn has no local tangent")
    tangent /= length
    normal = np.array([-tangent[1], tangent[0]])
    corners = xy + np.array([[-0.8, -1], [0.8, -1], [0.8, 1], [-0.8, 1]]) @ np.vstack(
        (normal, tangent)
    )
    return shapely.Polygon(corners)


def plan_supports(
    candidates, lower, upper, excluded, *, method, seconds=20, builder=None
):
    if (
        method not in ("fixed", "shifted")
        or not 0 < seconds <= 60
        or len(candidates) > 1024
    ):
        raise ValueError("bounded known support station method required")
    began = time.perf_counter()
    if builder is None:
        builder = SupportVolumeBuilder(lower.mesh, upper.mesh)
    shapes, products, contacts, ledger = [], [], [], []
    work = {
        "trials": 0,
        "source_face_candidates": 0,
        "noded_segments": 0,
        "contact_faces": 0,
        "accepted_pair_checks": 0,
    }
    shifts = (0,) if method == "fixed" else (0, -3, 3, -6, 6)
    shapely.prepare(excluded)
    for candidate in candidates:
        row = {k: v for k, v in candidate.items() if k != "line"}
        row.update(valid=False, trials=[])
        for shift in shifts:
            if time.perf_counter() - began >= seconds or work["trials"] >= 5120:
                row["reason"] = "support_time_or_trial_budget"
                break
            trial = {"shift_m": shift}
            work["trials"] += 1
            try:
                shape = footprint_at(candidate["line"], candidate["station_m"] + shift)
                if shapely.intersects(excluded, shape):
                    raise ValueError("support_lower_traffic_or_portal_exclusion")
                work["accepted_pair_checks"] += len(shapes)
                if work["accepted_pair_checks"] > 1048576:
                    raise ValueError("support_pair_budget")
                if shapes and np.any(shapely.intersects(shape, shapes)):
                    raise ValueError("support_existing_pier_conflict")
                product, note = builder.build(shape, minimum_height_m=2.0)
                if product is None:
                    raise ValueError(note["reason"])
                body, caps = (
                    product.body,
                    (product.lower_contact, product.upper_contact),
                )
                for key in (
                    "source_face_candidates",
                    "noded_segments",
                    "contact_faces",
                ):
                    work[key] += note[key]
                row.update(
                    valid=True,
                    reason="",
                    shift_m=shift,
                    product=len(products),
                    geometry=note,
                )
                products.append(body)
                contacts.append(caps)
                shapes.append(shape)
                trial["reason"] = "accepted"
            except (ValueError, shapely.errors.GEOSException) as failure:
                trial["reason"] = str(failure)
            row["trials"].append(trial)
            if row["valid"]:
                break
            row["reason"] = trial["reason"]
        ledger.append(row)
    return (
        products,
        contacts,
        shapes,
        {
            "method": method,
            "stations": ledger,
            "work": work,
            "seconds": time.perf_counter() - began,
        },
    )
