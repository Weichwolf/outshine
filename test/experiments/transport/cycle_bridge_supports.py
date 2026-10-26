"""Bounded local pier rhythm using the existing whole-contact reference kernel."""

import time

import numpy as np
import shapely
from cycle_bridge_problem import band
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from surface_solid_checks import audit_closed


def supports(fixture, problem, ribbon):
    field = Mesh(np.array(fixture["field_vertices"]), np.array(fixture["field_faces"]))
    builder = SupportVolumeBuilder(field, ribbon.lower)
    obstacles = shapely.union_all(
        [
            band(r)
            for r in fixture["sources"]
            if r["role"] in ("underpass", "ground_joint")
        ]
    ).buffer(0.2)
    origin, axis = problem["origin"], problem["axis"]
    normal = np.array([-axis[1], axis[0]])
    ext = (shapely.get_coordinates(ribbon.shape) - origin) @ axis
    stations = np.arange(ext.min() + 8, ext.max() - 4, 12)
    if len(stations) > 64:
        raise ValueError("cycle bridge support station budget")
    products, records, chosen = [], [], []
    began = time.monotonic()
    for nominal in stations:
        trials = []
        for shift in (0, -2, 2, -4, 4):
            if time.monotonic() - began > 8:
                raise ValueError("cycle bridge support time budget")
            station = nominal + shift
            centre = origin + axis * station
            points = np.array(
                [[-0.4, -0.225], [0.4, -0.225], [0.4, 0.225], [-0.4, 0.225]]
            )
            shape = shapely.Polygon(points @ np.array([axis, normal]) + centre)
            reason = ""
            if not ribbon.shape.covers(shape):
                reason = "outside_whole_deck"
            elif shape.intersects(obstacles):
                reason = "other_ground_traffic_or_joint"
            else:
                product, note = builder.build(shape, minimum_height_m=0.1)
                if product is None:
                    reason = note["reason"]
            trials.append({"shift_m": shift, "reason": reason})
            if not reason:
                if audit_closed(product.body) <= 0:
                    raise ValueError("nonpositive support body")
                products.append(product)
                chosen.append(station)
                records.append(
                    {
                        "nominal_m": float(nominal),
                        "station_m": float(station),
                        "product": len(products) - 1,
                        "trials": trials,
                        "geometry": note,
                    }
                )
                break
        else:
            records.append(
                {"nominal_m": float(nominal), "product": None, "trials": trials}
            )
    gap = float(np.max(np.diff(np.r_[ext.min(), sorted(chosen), ext.max()])))
    if gap > 24 or not products:
        raise ValueError("cycle bridge procedural support span budget")
    return products, {
        "stations": records,
        "maximum_station_gap_m": gap,
        "seconds": time.monotonic() - began,
        "scope": "Whole-contact procedural piers; spacing is a recipe, not a structural calculation.",
    }
