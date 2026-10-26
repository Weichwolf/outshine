"""Limited geometric association for field diagnostics; native Face IDs stay unknown."""

from collections import Counter

import numpy as np
from roof_prisms import shape_planes


def associate(source, recipes):
    faces = np.full(len(source["indices"]), -1, np.int32)
    reasons = np.zeros(len(faces), np.uint8)
    triangles = source["positions"][source["indices"]].astype(float)
    snap_bound = np.sqrt(3) * 0.0005
    for index in np.unique(source["binding"][:, 1]):
        ids = np.flatnonzero(source["binding"][:, 1] == index)
        surface = recipes[int(index)]
        if surface is None or len(surface["shapes"]) != 1:
            reasons[ids] = 1
            continue
        shape = surface["shapes"][0]
        # Use the already supplied geometry controls, not observed UV labels.
        geometry = {
            **shape,
            "height_controls": [],
            "roof_deck_m": shape["seat_m"]
            + shape["foot_m"]
            + shape["eaves_m"]
            + (shape["rise_m"] if shape["roof_kind"] == 0 else 0),
        }
        geometry["bottom_m"] = (
            shape["sole_m"]
            if shape["foot_m"] == 0
            else (
                max(
                    surface["minimum_height_m"], shape["seat_m"] + shape["foot_m"] - 0.3
                )
                if surface["minimum_height_m"] != 0
                else shape["seat_m"] + shape["foot_m"] - 0.3
            )
        )
        # shape_planes consumes already exported band/facet plans; added by caller.
        geometry.update(surface["geometry_plans"][0])
        try:
            planes, _, roof = shape_planes(geometry)
        except ValueError:
            reasons[ids] = 2
            continue
        basis = np.array([surface[k] for k in ("east", "north", "up")])
        world = triangles[ids]
        local = (world - surface["origin"]) @ basis.T
        n = np.cross(local[:, 1] - local[:, 0], local[:, 2] - local[:, 0])
        float_error = np.linalg.norm(
            np.spacing(abs(world).astype(np.float32)).astype(float).max(1) / 2, axis=1
        )
        tolerance = snap_bound + float_error + 1e-9
        residual = np.abs(
            np.einsum("tvi,pi->tvp", local, planes[:, :3]) - planes[:, 3]
        ).max(1)
        match = (residual <= tolerance[:, None]) & (n @ planes[:, :3].T > 0)
        semantic = np.arange(len(planes), dtype=np.int32) + 1
        semantic[0] = 0
        semantic[roof] = 1
        for at, row in enumerate(match):
            labels = np.unique(semantic[row])
            if len(labels) == 1:
                faces[ids[at]] = labels[0]
            else:
                reasons[ids[at]] = 3 if not len(labels) else 4
    return faces, reasons, Counter(map(int, reasons))


def with_geometry_plans(recipes, document):
    """Keep measured exported roof domains distinct from decoded extra fields."""
    result = {}
    for item in document["sources"]:
        original = recipes[item["index"]]
        if original is None:
            result[item["index"]] = None
            continue
        plans = []
        for shape in item["surface"]["shapes"]:
            plans.append({k: shape[k] for k in ("roof_bands", "hip") if k in shape})
        result[item["index"]] = {**original, "geometry_plans": plans}
    return result
