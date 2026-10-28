"""Opening-side guards with traffic portals and posts anchored in actual plates."""

import numpy as np
import shapely
from indoor_rail_clearance import RailClearance
from indoor_rail_geometry import hollow_sweep
from indoor_rail_supports import SupportLimits


def trim_terminal_stubs(piece, minimum):
    if np.array_equal(piece[0], piece[-1]):
        return piece
    while len(piece) > 2 and np.linalg.norm(piece[1] - piece[0]) < minimum:
        piece = piece[1:]
    while len(piece) > 2 and np.linalg.norm(piece[-1] - piece[-2]) < minimum:
        piece = piece[:-1]
    return piece


def guard_posts(path, floor, index, rail_limits, support_limits):
    distances = np.linalg.norm(np.diff(path, axis=0), axis=1)
    cumulative = np.r_[0, np.cumsum(distances)]
    length = float(cumulative[-1])
    inset = support_limits.end_inset_m
    if length <= 2 * inset:
        return None, "guard_piece_too_short_for_posts"
    count = max(2, int(np.ceil(length / (support_limits.maximum_pitch_m * 0.8))) + 1)
    if count > support_limits.max_posts:
        raise ValueError("guard_post_budget")
    posts, accepted = [], []
    for at in np.linspace(inset, length - inset, count):
        segment = min(
            len(distances) - 1, int(np.searchsorted(cumulative, at, side="right") - 1)
        )
        t = (at - cumulative[segment]) / distances[segment]
        centre = path[segment] * (1 - t) + path[segment + 1] * t
        tangent = (path[segment + 1, :2] - path[segment, :2]) / distances[segment]
        normal = np.array([-tangent[1], tangent[0]])
        corners = (
            np.array([[-1, -1], [1, -1], [1, 1], [-1, 1]]) * rail_limits.width_m / 2
        )
        footprint = shapely.Polygon(
            centre[:2] + corners @ np.column_stack((tangent, normal)).T
        )
        bottom = centre.copy()
        bottom[2] = floor.upper_z_m - support_limits.embed_m
        if bottom[2] < floor.lower_z_m:
            return None, "guard_embed_below_actual_plate"
        if not floor.polygon.covers(footprint) or index.blocked(bottom, centre):
            continue
        height = centre[2] - bottom[2]
        transform = np.column_stack(
            ((0, 0, height), np.r_[tangent, 0.0], np.r_[normal, 0.0], bottom)
        )
        posts.append(
            {
                "kind": "post",
                "transform": transform,
                "height_m": height,
                "bottom_z_m": bottom[2],
                "top_z_m": centre[2],
                "station_m": float(at),
            }
        )
        accepted.append(at)
    if (
        not accepted
        or accepted[0] > support_limits.maximum_cantilever_m
        or length - accepted[-1] > support_limits.maximum_cantilever_m
        or np.diff(accepted).max(initial=0) > support_limits.maximum_pitch_m
    ):
        return None, "guard_post_support_unplanned"
    return posts, ""


def opening_guards(batch, floor_reservations, rail_limits, *, support_limits=None):
    support_limits = SupportLimits() if support_limits is None else support_limits
    index = RailClearance(
        (*floor_reservations, *batch["reservations"]),
        rail_limits.width_m,
        rail_limits.clearance_m,
    )
    floors = {f.identifier: f for f in batch["floors"]}
    result, ledger = [], []
    for opening in batch["openings"]:
        floor = floors[opening["floor_id"]]
        geometry = shapely.from_wkb(bytes.fromhex(opening["wkb_hex"]))
        for ordinal, part in enumerate(shapely.get_parts(geometry)):
            owners = tuple(
                s["source_id"]
                for s in batch["stairs"]
                if s["footprint"].intersection(part).area > 0
            )
            outline = part.buffer(
                index.radius_xy + rail_limits.offset_margin_m, join_style="mitre"
            ).exterior
            # A real generated-path simplification, before traffic clipping;
            # original opening/plate geometry is never changed.
            outline = outline.simplify(1e-7, preserve_topology=True)
            xy = shapely.get_coordinates(outline)
            path = np.c_[
                xy, np.full(len(xy), floor.upper_z_m + rail_limits.handrail_height_m)
            ]
            pieces = index.clip(path)
            records = []
            reason = ""
            for piece_index, piece in enumerate(pieces):
                piece = trim_terminal_stubs(
                    piece, 2 * rail_limits.width_m + rail_limits.clearance_m
                )
                if (
                    len(piece) < 2
                    or np.linalg.norm(np.diff(piece, axis=0), axis=1).sum()
                    < rail_limits.minimum_piece_m
                ):
                    continue
                try:
                    mesh, audit = hollow_sweep(
                        piece[:, :2],
                        rail_limits.width_m,
                        rail_limits.wall_m,
                        origin=(0, 0, path[0, 2]),
                        closed=np.array_equal(piece[0], piece[-1]),
                    )
                except ValueError as error:
                    reason = str(error)
                    break
                posts, reason = guard_posts(
                    piece, floor, index, rail_limits, support_limits
                )
                if reason:
                    break
                for post in posts:
                    post["source_id"] = floor.identifier
                    post["guard_id"] = (
                        f"{floor.identifier}:opening:{ordinal}:piece:{piece_index}"
                    )
                records.append(
                    {
                        "identifier": f"{floor.identifier}:opening:{ordinal}:piece:{piece_index}",
                        "floor_id": floor.identifier,
                        "opening_ordinal": ordinal,
                        "source_ids": owners,
                        "world_path": piece,
                        "mesh": mesh,
                        "mesh_audit": audit,
                        "posts": posts,
                    }
                )
            if not records:
                reason = reason or "opening_guard_has_no_supported_piece"
            ledger.append(
                {
                    "floor_id": floor.identifier,
                    "opening_ordinal": ordinal,
                    "source_ids": owners,
                    "status": "pending" if reason else "candidate",
                    "reason": reason,
                }
            )
            if not reason:
                result.extend(records)
    return {
        "guards": tuple(result),
        "ledger": ledger,
        "traffic_tests": index.tests,
        "scope": "Candidate opening guards with traffic portals; final emitted geometry/material/clearance/source checks still required.",
    }
