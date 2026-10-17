"""Level bounded new junction regions at DEM priors; preserve cached contacts."""

import numpy as np
import shapely
from stair_landing_pads import node_profiles
from stair_module_sections import interpolate
from surface_height_checks import normal_gradients


def plan_heights(rows, nodes, cached_points, *, include_interiors=False):
    result = []
    for node in nodes:
        if any(np.array_equal(node["xy"], p) for p in cached_points):
            continue
        profiles = [
            p
            for _i, _v, p in node_profiles(
                rows, node, include_interiors=include_interiors
            )
        ]
        heights = []
        for i in node["incident"]:
            row = rows[i]
            matches = np.flatnonzero(np.all(row["xy"] == node["xy"], axis=1))
            if len(matches) != 1:
                raise ValueError("one original source sample at each new node required")
            heights.append(row["prior"][matches[0]])
        if not 2 <= len(profiles) <= 4 or np.ptp(heights) > 1e-10:
            raise ValueError(
                "bounded shared whole profiles and consistent node DEM prior required"
            )
        reach = max(rows[i]["width_m"] for i in node["incident"]) / 2 + 0.75
        x, y = node["xy"]
        shape = shapely.box(x - reach, y - reach, x + reach, y + reach)
        result.append(
            {"mvt_xy": node["mvt_xy"], "height_m": float(heights[0]), "shape": shape}
        )
    return result


def apply_heights(xy, fixed, values, plans):
    fixed, values = fixed.copy(), values.copy()
    for plan in plans:
        selected = shapely.dwithin(plan["shape"], shapely.points(xy), 1e-8)
        common = fixed & selected
        if np.max(abs(values[common] - plan["height_m"]), initial=0) > 1e-7:
            raise ValueError(
                "new junction height conflicts with a frozen actual contact"
            )
        values[selected & ~fixed] = plan["height_m"]
        fixed |= selected
    return fixed, values


def inspect_heights(data, plans):
    result = []
    for cast in (False, True):
        vertices = (
            data["surface_vertices"].astype("f4").astype(float)
            if cast
            else data["surface_vertices"]
        )
        faces = data["surface_faces"]
        triangles = vertices[faces]
        polygons = shapely.polygons(triangles[:, :, :2])
        for plan in plans:
            cuts = shapely.intersection(polygons, plan["shape"])
            selected = shapely.area(cuts) > 1e-12
            if not selected.any():
                raise ValueError("planned junction lacks an actual surface region")
            xy, parents = shapely.get_coordinates(cuts[selected], return_index=True)
            actual = triangles[selected][parents]
            heights = interpolate(actual, xy, actual[:, :, 2])
            datum = float(np.float32(plan["height_m"])) if cast else plan["height_m"]
            used = np.unique(faces[selected])
            xy_error = float(
                np.linalg.norm(
                    vertices[used, :2] - data["surface_vertices"][used, :2], axis=1
                ).max()
            )
            slope = float(
                np.linalg.norm(
                    normal_gradients(vertices[:, :2], faces[selected], vertices[:, 2]),
                    axis=1,
                ).max()
            )
            guard = 1e-7 + slope * (xy_error + 1e-7)
            error = float(abs(heights - datum).max())
            if error > guard:
                raise ValueError("whole actual new junction region is not level")
            result.append(
                {
                    "float32": cast,
                    "mvt_xy": plan["mvt_xy"],
                    "height_m": datum,
                    "maximum_height_error_m": error,
                    "z_guard_m": guard,
                    "actual_projected_area_m2": float(shapely.area(cuts).sum()),
                }
            )
    return result
