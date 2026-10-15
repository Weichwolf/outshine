"""Camera-independent ceiling fixtures near existing circulation portals."""

import numpy as np
import shapely


def additional(current, index, base):
    room = current["rooms"][index]
    centres = [np.asarray(base["position"][:2])]
    result = []
    for door in sorted(current["doors"], key=lambda row: row["id"]):
        if index not in door["rooms"]:
            continue
        centre = np.asarray(door["centre"])
        tangent = np.asarray(door["tangent"])
        normal = np.array([-tangent[1], tangent[0]])
        choices = [centre + sign * normal * 0.55 for sign in (-1, 1)]
        inside = [p for p in choices if room.covers(shapely.Point(p))]
        if not inside:
            continue
        point = max(inside, key=lambda p: room.boundary.distance(shapely.Point(p)))
        if any(np.linalg.norm(point - p) < 1.2 for p in centres):
            continue
        clearance = room.boundary.distance(shapely.Point(point))
        size = min(0.8, float(clearance * np.sqrt(2.0) * 0.8))
        if size <= 0.02:
            continue
        centres.append(point)
        result.append(
            base
            | {
                "fixture_id": base["room_id"] + "/ceiling/portal/" + door["id"],
                "portal_id": door["id"],
                "position": [*point.tolist(), base["position"][2]],
                "size": [size, size],
                "origin": "Procedural circulation ceiling fixture at a physical portal; independent of camera and leaf state",
            }
        )
        if len(result) == 32:
            break
    return result
