"""Independent whole internal endpoint contact on actual owned saved cap faces."""

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_cycle_contacts import profile
from stair_junction_layout import landings


def cap_proof(data, rounded, row, end, owner, neighbours, mask):
    sides = []
    for bits in (1 << owner, sum(1 << j for j in neighbours)):
        faces = data["top_faces"][(data["face_sources"] & bits) != 0]
        shapes = shapely.polygons(data["body_vertices"][faces, :2])
        sides.append(faces[shapely.dwithin(mask, shapely.centroid(shapes), 1e-8)])
    if any(not len(faces) for faces in sides):
        raise ValueError("chosen landing lacks actual owner or neighbour faces")
    line = profile(row, end)
    checks = []
    for cast in (False, True):
        vertices = rounded if cast else data["body_vertices"]
        points = line.astype("f4").astype(float) if cast else line
        guard = 2 * float(abs(np.spacing(points.astype("f4"))).max()) if cast else 1e-7
        levels = []
        for faces in sides:
            cap = vertices[faces]
            polygons = shapely.polygons(cap[:, :, :2])
            keep = shapely.dwithin(shapely.LineString(points), polygons, guard)
            chosen = cap[keep]
            if not len(chosen) or np.ptp(chosen[:, :, 2]) > 1e-7:
                raise ValueError("actual whole internal landing cap is not flat")
            cover = shapely.union_all(shapely.polygons(chosen[:, :, :2]))
            missing = shapely.LineString(points).difference(cover.buffer(guard)).length
            if missing > 1e-7:
                raise ValueError("actual landing misses a whole internal profile")
            levels.append(float(chosen[0, 0, 2]))
        if abs(levels[0] - levels[1]) > 1e-7:
            raise ValueError("actual owner and neighbour profile heights differ")
        checks.append(
            {
                "float32": cast,
                "owner_neighbour_heights_m": levels,
                "xy_guard_m": guard,
                "uncovered_length_m": float(missing),
            }
        )
    return checks


def node_profiles(data, rows, nodes):
    domains = [corridor(row) for row in rows]
    _, owners, masks = landings(rows, domains, "width", 0.75, nodes)
    rounded = data["body_vertices"].astype("f4").astype(float)
    profiles, outer, unresolved = [], [], []
    for i, row in enumerate(rows):
        for end in (0, -1):
            point = shapely.Point(row["xy"][end])
            choices = [
                j
                for j, (ids, mask) in enumerate(zip(owners, masks, strict=True))
                if i in ids
                and mask.distance(point) < 1e-7
                and any(domains[k].distance(point) < 1e-7 for k in ids if k != i)
            ]
            record = {"source_id": row["source_id"], "end": end}
            if not choices:
                outer.append(record)
                continue
            errors = []
            for j in choices:
                neighbours = [k for k in owners[j] if k != i]
                try:
                    checks = cap_proof(data, rounded, row, end, i, neighbours, masks[j])
                except ValueError as error:
                    errors.append(str(error))
                    continue
                profiles.append(
                    dict(
                        record,
                        node_mvt_xy=nodes[j]["mvt_xy"],
                        neighbours=[rows[k]["source_id"] for k in neighbours],
                        whole_cap_checks=checks,
                    )
                )
                break
            else:
                unresolved.append(dict(record, reasons=errors))
    return {
        "profiles": profiles,
        "outer_profiles": outer,
        "unresolved_internal_profiles": unresolved,
        "complete_internal_profiles": len(profiles),
        "all_chosen_internal_profiles_whole": not unresolved,
    }
