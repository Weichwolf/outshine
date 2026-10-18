"""Original-node shared paving: road geometry does not grant motor access to a path."""

import numpy as np
import shapely
from stair_module_inputs import reference_profiles
from stair_module_roads import face_frames
from stair_surface_access import ROAD_FOOT_CONTACTS
from surface_height_checks import normal_gradients


def paving_node(state, recipe, sources, chosen):
    key, old_key = chosen["source_id"], chosen["cached_source_id"]
    source, old = sources[key], sources[old_key]
    if (
        recipe.get("source_profiles", {}).get(key) != "unclassified"
        or len(source["xy_m"]) != 2
        or "road_axis" in recipe
    ):
        raise ValueError("shared paving needs an explicit straight minor profile")
    root = recipe["port"]
    root_key = root["source_id"]
    if root_key == old_key or sources[root_key]["properties"].get("class") != "minor":
        raise ValueError("shared paving needs its distinct actual minor root")
    index = root.get("vertex", root.get("end"))
    node = sources[root_key]["mvt_xy"][index]
    point = sources[root_key]["xy_m"][index]
    for selected in (source, old):
        matches = [i for i, n in enumerate(selected["mvt_xy"]) if n == node]
        if len(matches) != 1 or not np.array_equal(selected["xy_m"][matches[0]], point):
            raise ValueError("shared paving requires the same original node and frame")
    row = state["cached_module_rows"][old_key]
    if not np.array_equal(row["xy"], old["xy_m"]):
        raise ValueError("shared footway axis differs from its immutable source")
    width = row.get("width_m")
    if width != reference_profiles()["footway"]["widthM"]:
        raise ValueError("shared paving requires its verified whole footway width")
    return {"original_shared_node_mvt": node, "walk_width_m": width}


def cap_frames(cap, row, shape):
    polygons = shapely.polygons(cap.vertices[cap.triangles, :2])
    q = cap.vertices.astype("f4").astype(float)
    quantized = shapely.polygons(q[cap.triangles, :2])
    selected = (shapely.area(shapely.intersection(polygons, shape)) > 0) | (
        shapely.area(shapely.intersection(quantized, shape)) > 0
    )
    faces = cap.triangles[selected]
    if not len(faces):
        return [], 0
    frames = face_frames(
        [row], cap.vertices[:, :2], faces, np.ones((len(faces), 1), bool)
    )
    checks = []
    for cast in (False, True):
        vertices = q if cast else cap.vertices
        gradient = normal_gradients(vertices[:, :2], faces, vertices[:, 2])
        maximum = float(
            np.linalg.norm(np.einsum("fij,fj->fi", frames, gradient), axis=1).max()
        )
        if not np.isfinite(maximum) or maximum > 1:
            raise ValueError("borrowed whole paving exceeds new road profile frame")
        checks.append(
            {"float32": cast, "facets": len(faces), "maximum_normalized_grade": maximum}
        )
    return checks, len(faces)


def inspect_paving(state, rows, original, root_cap):
    owners = sorted(
        {
            c["owner"]
            for c in state.get("surface_contacts", [])
            if c["kind"] in ROAD_FOOT_CONTACTS
        }
    )
    report, counted = [], 0
    for owner in owners:
        row = rows[owner]
        caps = (
            [root_cap]
            + [
                c["cap"]
                for c in state["module_contacts"]
                if row["source_id"] in c["incident_source_ids"]
            ]
            + [c["cap"] for c in state["surface_contacts"] if c["owner"] == owner]
        )
        checks = []
        for cap in caps:
            proof, facets = cap_frames(cap, row, original[owner])
            counted += facets
            if counted > 32768:
                raise ValueError("borrowed road paving frame budget exhausted")
            checks.append(proof)
        report.append({"source_id": row["source_id"], "borrowed_caps": checks})
    return report
