"""Independent whole-facet and prepared-patch checks, including actual Float32."""

import numpy as np
import shapely
from mvt_t_profile_layout import prior_values
from stair_junction_audit import audit


def audit_profiles(plan, heights, data, *, max_prior_change_m=2.0):
    if heights.shape != (len(plan["xy"]),) or not np.isfinite(heights).all():
        raise ValueError("finite height for every retained surface vertex required")
    grades, prior_error, landing_error = [], 0.0, 0.0
    transverse = {}
    for row, faces in zip(plan["rows"], plan["parts"], strict=True):
        points = np.c_[plan["xy"], heights][faces]
        normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        if np.any(normals[:, 2] <= 0):
            raise ValueError("positive whole-profile projected facets required")
        gradients = -normals[:, :2] / normals[:, 2:3]
        maximum = float(np.linalg.norm(gradients, axis=1).max())
        if maximum > row["gradient"] + 1e-7:
            raise ValueError("whole profile exceeds its declared grade")
        grades.append(maximum)
        if row["original"]["source"]["feature"]["properties"]["subclass"] == "steps":
            tangent = row["xy"][-1] - row["xy"][0]
            tangent /= np.linalg.norm(tangent)
            normal = np.array([-tangent[1], tangent[0]])
            crossfall = float(abs(gradients @ normal).max())
            if crossfall > 1e-7:
                raise ValueError(
                    "whole straight-flight transverse gradient is not level"
                )
            transverse[row["source_id"]] = crossfall
        vertices = np.unique(faces)
        prior_error = max(
            prior_error,
            float(
                abs(heights[vertices] - prior_values(row, plan["xy"][vertices])).max()
            ),
        )
    if prior_error > max_prior_change_m + 1e-7:
        raise ValueError("whole retained finite height prior budget exceeded")
    for mask, owners in zip(plan["masks"], plan["incident"], strict=True):
        reference = None
        for owner in owners:
            points = np.c_[plan["xy"], heights][plan["parts"][owner]]
            cuts = shapely.intersection(shapely.polygons(points[:, :, :2]), mask)
            selected = np.flatnonzero(shapely.area(cuts) > 1e-12)
            for index in selected:
                tri = points[index]
                normal = np.cross(tri[1] - tri[0], tri[2] - tri[0])
                gradient = -normal[:2] / normal[2]
                xy = shapely.get_coordinates(cuts[index])
                values = tri[0, 2] + (xy - tri[0, :2]) @ gradient
                if reference is None:
                    reference = float(values[0])
                landing_error = max(landing_error, float(abs(values - reference).max()))
        if reference is None:
            raise ValueError("prepared landing has no actual retained facets")
    if landing_error > 1e-7:
        raise ValueError("whole prepared landing is not one shared level plane")
    body = audit(
        data,
        plan["prepared_domains"],
        limits=[r["gradient"] for r in plan["rows"]],
        crossings=[],
    )
    return {
        "maximum_prior_error_m": prior_error,
        "maximum_whole_grades": grades,
        "whole_flight_transverse_gradients": transverse,
        "whole_landing_error_m": landing_error,
        "body": body,
    }
