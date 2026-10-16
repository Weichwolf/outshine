"""Present the already validated physical near assembly when its openings resolve."""

import numpy as np
import shapely
from demand import displacement_error
from facade_surface import coordinates
from opening_details import slots

DETAIL_RADIUS_M = 0.25
PHYSICAL_OPENING_PX = 8.0


def corners(opening):
    centre, tangent = np.asarray(opening["centre"]), np.asarray(opening["tangent"])
    low = opening["bottom_z_m"]
    return np.array(
        [
            [*(centre + tangent * u), z]
            for u in (-opening["width_m"] / 2, opening["width_m"] / 2)
            for z in (low, low + opening["height_m"])
        ]
    )


def demand(plan, view, *, max_openings=512):
    openings = slots(plan)
    if type(max_openings) is not int or not 0 <= len(openings) <= max_openings:
        raise ValueError("Stepped opening capacity exceeded")
    decisions = []
    for opening in openings:
        points = corners(opening)
        q = (points - view.eye) @ view.basis.T
        distances = q @ view.planes[:, :3].T + view.planes[:, 3]
        margin = (view.movement_m + DETAIL_RADIUS_M) * np.linalg.norm(
            view.planes[:, :3], axis=1
        )
        visible = not np.any(distances.max(axis=0) + margin < 0)
        error = displacement_error(points, view, DETAIL_RADIUS_M)
        nearest = float(q[:, 2].min()) - view.movement_m
        width = (
            float("inf")
            if nearest <= 0
            else view.focal_px * opening["width_m"] / nearest
        )
        decisions.append(
            {
                "id": opening["id"],
                "resolved": visible
                and error > view.detail_error_px
                and width >= PHYSICAL_OPENING_PX,
                "component_bound_px": error,
                "projected_width_bound_px": width,
            }
        )
    inside = plan["base_z_m"] <= view.eye[2] <= plan["source_top_m"] and plan[
        "footprint"
    ].covers(shapely.Point(view.eye[:2]))
    return inside or any(d["resolved"] for d in decisions), {
        "inside_source": bool(inside),
        "decisions": decisions,
    }


def present(product, wall_material, glass_material, *, max_triangles=65536):
    if type(max_triangles) is not int or max_triangles < len(product["faces"]):
        raise ValueError("Stepped near presentation triangle capacity exceeded")
    expected = np.arange(
        len(product["vertices"]), dtype=product["faces"].dtype
    ).reshape(-1, 3)
    if not np.array_equal(product["faces"], expected):
        raise ValueError("Near presentation requires independent triangle corners")
    rows, uv, _ = coordinates(product, (1.0, 1.0), material=3)
    output = dict(product)
    output["uv"] = product["uv"].copy()
    output["uv"].reshape(-1, 3, 2)[rows] = uv
    output["materials"] = product["materials"].copy()
    output["materials"][product["materials"] == 1] = wall_material
    output["materials"][product["materials"] == 3] = glass_material
    output["facade_extent"] = np.zeros((len(product["vertices"]), 3))
    return output


def opening_records(plan):
    return [
        opening
        | {
            "first_triangle": None,
            "outside_offset_m": None,
            "scope": "Actual shared near assembly; this logical opening does not occupy one contiguous triangle range",
        }
        for opening in slots(plan)
    ]
