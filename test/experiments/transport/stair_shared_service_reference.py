"""Independent per-direction straight-cap opponent for the shared-service control."""

import numpy as np
import shapely
from stair_service_vehicle import straight_spans
from transport_vehicle_sweep import straight_body_check


def separate_caps(data, row, vehicle):
    """Check the same two straight directions, rebuilding each route's cap.

    Restricted to the declared shared-service straight control. Curved body
    correctness is checked separately by the existing bounded sweep reference.
    """
    if row.get("horizontal_curve_plan", {}).get("corners"):
        raise ValueError("The separate-cap cost control requires a straight source")
    result = []
    for cast in (False, True):
        for reverse in (False, True):
            xyz = data["surface_vertices"]
            if cast:
                xyz = xyz.astype("f4").astype(float)
            cap = shapely.union_all(
                shapely.polygons(xyz[data["surface_faces"][data["owners"][:, 0]], :2])
            )
            begin, finish, heading, inset = straight_spans(
                row, vehicle, reverse=reverse
            )
            sweeps = []
            for a, b, theta in zip(begin, finish, heading, strict=True):
                check, _ = straight_body_check(cap, a, b, theta, vehicle)
                if not check["fits_surface"]:
                    raise ValueError("Independent opposing body leaves the stored cap")
                sweeps.append(dict(check, begin_xy_m=a.tolist(), end_xy_m=b.tolist()))
            result.append(
                {
                    "source_id": row["source_id"],
                    "float32": cast,
                    "source_oneway": row["traffic_profile"]["source_oneway"],
                    "lane_offset_m": 0.0,
                    "straight_sweeps": sweeps,
                    "source_end_rear_axle_insets_m": list(inset),
                    "reverse": reverse,
                    "opposing_traffic_policy": "exclusive_lane_occupancy",
                }
            )
    return result


def equal_checks(expected, actual):
    if len(expected) != len(actual):
        raise ValueError("Opposing body check count differs")
    for a, b in zip(expected, actual, strict=True):
        if a != b:
            raise ValueError("Independent complete body checks differ")


def unchanged_assets(before, after):
    if len(after) != len(before) + 1:
        raise ValueError("One complete source must append one physical road asset")
    arrays = bytes_checked = 0
    for old, current in zip(before, after[:-1], strict=True):
        if old.identity != current.identity or old.data.keys() != current.data.keys():
            raise ValueError("Old physical asset identity changed")
        for key, value in old.data.items():
            np.testing.assert_array_equal(value, current.data[key])
            arrays += 1
            bytes_checked += value.nbytes
    return {
        "old_assets": len(before),
        "old_arrays": arrays,
        "old_array_bytes": bytes_checked,
    }
