"""Bounded inferred crowns for previously flat large ordinary source bodies."""

import shapely
from outshine_ref.buildings.structures import classify_structure
from provider_contract import attributes, number, seed_for
from roof_crown_plan import plan
from tile_facades import recipe_for
from tile_roof_envelopes import choose
from tile_room_details import numeric_properties


def prepare(row):
    parameters = attributes(numeric_properties(row))
    p = row["polygon"]
    if row.get("clipped") or parameters["minHeight"] != 0 or parameters["hide3d"]:
        return None, "clipped, raised or hidden source"
    if not 800 < p.area <= 6000 or not 4.5 <= parameters["buildingHeight"] <= 60:
        return None, "outside large ordinary crown area/height scope"
    if len(shapely.get_coordinates(p)) > 80:
        return None, "source exceeds 80 boundary coordinates"
    if any(
        k.startswith("roof") and v is not None for k, v in row["properties"].items()
    ):
        return None, "supplied roof fields retained"
    if choose(row, parameters).kind != "flat":
        return None, "existing nonflat roof programme retained"
    if (
        classify_structure(
            p, row["properties"], coordinate_step_m=row["coordinate_step_m"]
        ).kind
        != "ordinary"
    ):
        return None, "special-structure programme retained"
    if recipe_for(row, parameters)["family"] not in (
        "residential",
        "mixed-use",
        "commercial",
        "institutional",
    ):
        return None, "existing ordinary/special family retained"
    levels = next(
        (
            number(row["properties"][k])
            for k in ("building:levels", "building_levels", "levels")
            if row["properties"].get(k) is not None
        ),
        None,
    )
    minimum = max(1.0, levels if levels is not None else 1.0) * 2.64
    kind = (
        "parapet-service" if seed_for(row["id"] + "/roof-crown") % 3 == 0 else "parapet"
    )
    return plan(
        p, parameters["buildingHeight"], kind, body_min_m=minimum
    ), "inferred source-contained crown"
