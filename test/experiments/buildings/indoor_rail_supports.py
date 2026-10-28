"""Bounded post and below-tread bracket recipes for whole handrail support."""

from dataclasses import dataclass

import numpy as np
import shapely
from indoor_rail_clearance import RailClearance
from indoor_rail_recipe import stair_pose


@dataclass(frozen=True)
class SupportLimits:
    maximum_pitch_m: float = 1.2
    maximum_cantilever_m: float = 0.25
    end_inset_m: float = 0.05
    embed_m: float = 0.08
    bracket_width_m: float = 0.04
    bracket_inset_m: float = 0.06
    cap_fraction: float = 0.002
    max_posts: int = 1024

    def __post_init__(self):
        values = (
            self.maximum_pitch_m,
            self.maximum_cantilever_m,
            self.end_inset_m,
            self.embed_m,
            self.bracket_width_m,
            self.bracket_inset_m,
            self.cap_fraction,
        )
        if not np.isfinite(values).all() or min(values) <= 0:
            raise ValueError("Positive finite support recipe required")
        if (
            not self.end_inset_m < self.maximum_cantilever_m < self.maximum_pitch_m
            or self.cap_fraction >= 0.5
            or type(self.max_posts) is not int
            or not 2 <= self.max_posts <= 4096
        ):
            raise ValueError("Bounded support spacing and end-cap fraction required")


def nearest_station(stair, desired, minimum, maximum, inset):
    options = []
    for surface in stair["surfaces"]:
        low = max(minimum, surface["start_m"] + inset)
        high = min(maximum, surface["end_m"] - inset)
        if low <= high:
            at = float(np.clip(desired, low, high))
            options.append((abs(at - desired), at, surface))
    return min(options, key=lambda o: o[:2])[1:] if options else None


def support_plan(rail, stair, index, rail_limits, *, limits=None):
    limits = SupportLimits() if limits is None else limits
    profile = rail["profile"]
    minimum, maximum = float(profile[0, 0]), float(profile[-1, 0])
    if maximum - minimum <= 2 * limits.end_inset_m:
        return {"status": "pending", "reason": "handrail_piece_too_short_for_posts"}
    count = max(
        2, int(np.ceil((maximum - minimum) / (limits.maximum_pitch_m * 0.7))) + 1
    )
    if count > limits.max_posts:
        raise ValueError("rail_post_budget")
    desired = np.linspace(
        minimum + limits.end_inset_m, maximum - limits.end_inset_m, count
    )
    stations = {}
    for point in desired:
        chosen = nearest_station(
            stair,
            point,
            minimum + limits.end_inset_m,
            maximum - limits.end_inset_m,
            max(limits.end_inset_m, index.radius_xy),
        )
        if chosen is not None:
            stations[chosen[0]] = chosen[1]
    origin, basis = stair_pose(stair)
    lateral = float((rail["origin"] - origin) @ basis[:, 2])
    side = 1 if lateral > 0 else -1
    half = stair["mesh_audit"]["constructed_width_m"] / 2
    posts, brackets, accepted = [], [], []
    rejected = []
    for at, surface in sorted(stations.items()):
        if (
            limits.embed_m + limits.bracket_width_m
            > surface["minimum_vertical_waist_m"]
        ):
            return {"status": "pending", "reason": "bracket_anchor_below_actual_waist"}
        relative_top = float(np.interp(at, profile[:, 0], profile[:, 1]))
        xy = (origin + basis[:, 0] * at + basis[:, 2] * lateral)[:2]
        bottom = surface["z_m"] - limits.embed_m - limits.bracket_width_m / 2
        top = rail["origin"][2] + relative_top
        first, last = np.r_[xy, bottom], np.r_[xy, top]
        inner = (
            origin
            + basis[:, 0] * at
            + basis[:, 2] * side * (half - limits.bracket_inset_m)
        )
        outer = (
            origin
            + basis[:, 0] * at
            + basis[:, 2] * (lateral + side * rail_limits.width_m / 2)
        )
        inner[2] = outer[2] = surface["z_m"] - limits.embed_m
        inner_offset = basis[:2, 0] * limits.bracket_width_m / 2
        # Reserve the inner half of the actual embed as positive-volume contact,
        # physically inside the stair side face rather than on its boundary.
        body_edge = inner + basis[:, 2] * side * limits.bracket_inset_m / 2
        anchor = shapely.Polygon(
            (
                inner[:2] - inner_offset,
                body_edge[:2] - inner_offset,
                body_edge[:2] + inner_offset,
                inner[:2] + inner_offset,
            )
        )
        if not surface["polygon"].covers(anchor):
            return {
                "status": "pending",
                "reason": "bracket_anchor_contact_outside_tread",
            }
        if index.blocked(first, last) or index.blocked(inner, outer):
            rejected.append(at)
            continue
        height = top - bottom
        transform = np.column_stack(
            (basis[:, 1] * height, basis[:, 0], basis[:, 2], first)
        )
        posts.append(
            {
                "source_id": rail["source_id"],
                "kind": "post",
                "transform": transform,
                "bottom_z_m": bottom,
                "top_z_m": top,
                "height_m": height,
                "station_m": at,
            }
        )
        delta = outer - inner
        bracket = np.column_stack(
            (
                delta,
                -side * basis[:, 0] * limits.bracket_width_m,
                basis[:, 1] * limits.bracket_width_m,
                inner,
            )
        )
        brackets.append(
            {
                "source_id": rail["source_id"],
                "kind": "below_tread_bracket",
                "transform": bracket,
                "station_m": at,
            }
        )
        accepted.append(at)
    if not accepted:
        return {"status": "pending", "reason": "rail_supports_conflict_with_traffic"}
    cumulative = np.r_[0, np.cumsum(np.linalg.norm(np.diff(profile, axis=0), axis=1))]
    arc = np.interp(accepted, profile[:, 0], cumulative)
    if (
        arc[0] > limits.maximum_cantilever_m
        or cumulative[-1] - arc[-1] > limits.maximum_cantilever_m
        or np.diff(arc).max(initial=0) > limits.maximum_pitch_m
    ):
        return {
            "status": "pending",
            "reason": "rail_support_pitch_or_cantilever",
            "rejected_stations": rejected,
        }
    return {
        "status": "candidate",
        "reason": "",
        "posts": posts,
        "brackets": brackets,
        "rejected_stations": rejected,
        "maximum_pitch_m": float(np.diff(arc).max(initial=0)),
        "maximum_end_cantilever_m": float(max(arc[0], cumulative[-1] - arc[-1])),
    }


def handrail_supports(rails, batch, floor_reservations, rail_limits, *, limits=None):
    index = RailClearance(
        (*floor_reservations, *batch["reservations"]),
        rail_limits.width_m,
        rail_limits.clearance_m,
    )
    stairs = {s["source_id"]: s for s in batch["stairs"]}
    result = []
    for rail in rails:
        result.append(
            support_plan(
                rail, stairs[rail["source_id"]], index, rail_limits, limits=limits
            )
        )
    return result, index.tests
