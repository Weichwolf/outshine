"""Attach explicit longitudinal/crossfall recipes to retained source directions."""

import json
from pathlib import Path

import numpy as np
import shapely

from transport_building_clearance import street_kind


ROOT = Path(__file__).resolve().parents[3]
RECIPES = {"motorway": .06, "trunk": .06, "primary": .06, "secondary": .06, "tertiary": .06, "busway": .06,
           "minor": .08, "residential": .08, "service": .08, "rail": .03, "light_rail": .03, "tram": .03,
           "footway": .10, "pedestrian": .10, "cycleway": .10, "track": .10}


def frames_for_surface(xy, faces, roles, raw, used_ids):
    sources = sorted((r for r in raw["roads"] if r["source_id"] in used_ids), key=lambda r: r["source_id"])
    if not sources:
        raise ValueError("surface has no retained source direction")
    lines = np.array([shapely.LineString(r["coordinates"]) for r in sources], dtype=object)
    points = shapely.points(xy[faces].mean(axis=1))
    pairs = shapely.STRtree(lines).query_nearest(points, all_matches=True)
    order = np.lexsort((pairs[1], pairs[0]))
    pairs = pairs[:, order]
    counts = np.bincount(pairs[0], minlength=len(faces))
    first = np.r_[True, np.diff(pairs[0]) != 0]
    owners = pairs[1, first]
    if len(owners) != len(faces):
        raise ValueError("every road face needs a retained source direction")
    closest = lines[owners]
    stations = shapely.line_locate_point(closest, points)
    lengths = shapely.length(closest)
    before = shapely.get_coordinates(shapely.line_interpolate_point(closest, np.maximum(0., stations - .1)))
    after = shapely.get_coordinates(shapely.line_interpolate_point(closest, np.minimum(lengths, stations + .1)))
    tangent = after - before
    magnitude = np.linalg.norm(tangent, axis=1)
    uncertain = (magnitude == 0) | (counts > 1)
    np.divide(tangent, magnitude[:, None], out=tangent, where=magnitude[:, None] != 0)
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    longitudinal = np.array([RECIPES.get(street_kind(sources[i]["properties"]), .08) for i in owners])
    crossfall = np.full(len(faces), .025)
    frames = np.stack((tangent / longitudinal[:, None], normal / crossfall[:, None]), axis=1)
    frames[roles.astype(bool) | uncertain] = np.eye(2) / .025
    return frames, dict(source_ids=[sources[i]["source_id"] for i in owners],
                        recipes=RECIPES, default_longitudinal=.08, crossfall=.025, junction_grade=.025,
                        isotropic_uncertain_faces=np.flatnonzero(uncertain).tolist(),
                        note="Procedural recipe bounds, not observed tags. Nearest retained axis directs each corridor face; junctions and uncertain directions use a conservative isotropic bound. C1 and lane-specific superelevation remain open.")


def main():
    target_path = ROOT / "build/experiments/transport/heights/Vienna-targets.npz"
    with np.load(target_path) as data:
        xy, faces, targets = data["xy"], data["faces"], data["targets"]
    with np.load(ROOT / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz") as data:
        if not np.array_equal(data["vertices"], xy) or not np.array_equal(data["indices"], faces):
            raise ValueError("DEM targets and road direction owners belong to different surfaces")
        roles = data["part_roles"][data["face_owners"]]
    raw = json.loads((ROOT / "build/experiments/transport/inputs/Vienna-input.json").read_text())
    planned = json.loads((ROOT / "build/experiments/transport/flat-vienna/Vienna-flat-network.json").read_text())
    frames, report = frames_for_surface(xy, faces, roles, raw, set(planned["source_ids"]))
    np.savez_compressed(target_path.with_name("Vienna-profile-input.npz"), xy=xy, faces=faces, targets=targets, frames=frames)
    target_path.with_name("Vienna-profile-input.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(faces=len(faces), corridor_faces=int(np.count_nonzero(~roles.astype(bool))), junction_faces=int(np.count_nonzero(roles)), minimum_longitudinal=.03, crossfall=.025)))


if __name__ == "__main__":
    main()
