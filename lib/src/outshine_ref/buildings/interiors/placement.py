"""Bounded 2D activity-aware placements, retaining portal access before mesh expansion."""

import hashlib
from dataclasses import dataclass

import numpy as np
import shapely
from shapely.affinity import affine_transform

from ...contracts import geometry
from ...contracts import identifier as check_identifier
from .geometry import AGENT_RADIUS_M, edges, polygons
from .routes import routes


@dataclass(frozen=True, slots=True)
class Furnishing:
    kind: str
    width_m: float
    depth_m: float
    height_m: float
    activity_depth_m: float

    def __post_init__(self):
        values = (self.width_m, self.depth_m, self.height_m, self.activity_depth_m)
        if (
            not isinstance(self.kind, str)
            or not self.kind.strip()
            or any(isinstance(v, (bool, str)) for v in values)
            or not np.isfinite(values).all()
            or min(values) <= 0
        ):
            raise ValueError(
                "Furnishing dimensions and activity clearance must be finite and positive"
            )


def _owned_coordinates(value):
    value = np.asarray(value, dtype=float)
    if value.shape != (2,) or not np.isfinite(value).all():
        raise ValueError("Furnishing coordinates require a finite XY pair")
    return np.frombuffer(value.tobytes(), dtype=float)


def placed(spec, centre, tangent, identifier):
    check_identifier(identifier)
    if not isinstance(spec, Furnishing):
        raise TypeError("Placement requires a Furnishing specification")
    centre = np.asarray(centre, dtype=float)
    tangent = np.asarray(tangent, dtype=float)
    if (
        centre.shape != (2,)
        or tangent.shape != (2,)
        or not np.isfinite([*centre, *tangent]).all()
        or abs(tangent @ tangent - 1) > 1e-9
    ):
        raise ValueError(
            "Furnishing placement requires finite XY centre and unit tangent"
        )
    front = np.array([-tangent[1], tangent[0]])
    transform = [tangent[0], front[0], tangent[1], front[1], *centre]
    footprint = affine_transform(
        shapely.box(-spec.width_m / 2, 0.0, spec.width_m / 2, spec.depth_m), transform
    )
    if spec.kind == "meeting-group":
        local = shapely.box(-spec.width_m / 2, 0.0, spec.width_m / 2, spec.depth_m)
        activity_local = local.buffer(
            spec.activity_depth_m, join_style="mitre"
        ).difference(local)
        local_targets = [
            [-spec.width_m / 2 - spec.activity_depth_m / 2, spec.depth_m / 2],
            [spec.width_m / 2 + spec.activity_depth_m / 2, spec.depth_m / 2],
            [0.0, -spec.activity_depth_m / 2],
            [0.0, spec.depth_m + spec.activity_depth_m / 2],
        ]
    else:
        activity_local = shapely.box(
            -spec.width_m / 2,
            spec.depth_m,
            spec.width_m / 2,
            spec.depth_m + spec.activity_depth_m,
        )
        local_targets = [[0.0, spec.depth_m + spec.activity_depth_m / 2]]
    activity = affine_transform(activity_local, transform)
    target = centre + front * (spec.depth_m + spec.activity_depth_m / 2)

    return {
        "id": identifier,
        "kind": spec.kind,
        "spec": spec,
        "centre": _owned_coordinates(centre),
        "tangent": _owned_coordinates(tangent),
        "footprint": footprint,
        "activity": activity,
        "target": _owned_coordinates(target),
        "targets": tuple(
            _owned_coordinates(centre + tangent * x + front * y)
            for x, y in local_targets
        ),
    }


def access(room, portals, items, reserved=(), *, placement_reserved=()):
    geometry(room, polygonal=True)
    if room.geom_type != "Polygon" or len(shapely.get_coordinates(room)) > 256:
        raise ValueError(
            "Furnishing access requires one room with at most 256 ring coordinates"
        )
    if len(items) > 32 or len(portals) > 16:
        raise ValueError("Furnishing access exceeds its object or portal capacity")
    portal_points = np.asarray(portals, dtype=float)
    if (
        not len(portals)
        or portal_points.shape != (len(portals), 2)
        or not np.isfinite(portal_points).all()
    ):
        raise ValueError("Furnishing access requires finite XY portal anchors")
    placement_reserved = tuple(placement_reserved)
    if len(placement_reserved) > 16:
        raise ValueError("Furnishing movement reservation capacity exceeded")
    for region in placement_reserved:
        geometry(region, polygonal=True)
    obstacles = shapely.union_all([r["footprint"] for r in items])
    if any(
        not room.covers(r["footprint"]) or not room.covers(r["activity"]) for r in items
    ):
        raise ValueError("Furniture or activity area escapes its complete room polygon")
    for index, item in enumerate(items):
        if any(
            item["footprint"].intersection(other["footprint"]).area > 1e-9
            for other in items[:index]
        ):
            raise ValueError("Furnishing footprints overlap")
        if item["activity"].intersection(obstacles).area > 1e-9:
            raise ValueError("Furnishing activity area is occupied")
        if any(item["footprint"].intersects(region) for region in reserved):
            raise ValueError("Furnishing intersects a reserved approach or opening")
        if any(
            item[field].intersection(region).area > 1e-9
            for region in placement_reserved
            for field in ("footprint", "activity")
        ):
            raise ValueError("Furnishing or activity intersects reserved movement")
    free = room.difference(obstacles).buffer(-AGENT_RADIUS_M, join_style="mitre")
    anchors = [_owned_coordinates(p) for p in portals] + [
        _owned_coordinates(point) for r in items for point in r["targets"]
    ]
    if not anchors:
        raise ValueError("Furnished room has no actual portal anchor")
    component = next(
        (p for p in polygons(free) if all(p.covers(shapely.Point(a)) for a in anchors)),
        None,
    )
    if component is None:
        raise ValueError("Furniture disconnects a portal or required activity target")
    paths, report = routes(component, anchors, "doglegs")
    if paths is None:
        paths, report = routes(component, anchors, "orthogonal-grid")
    if paths is None:
        raise ValueError("Bounded route search cannot connect every furnished activity")
    return {
        "free": component,
        "paths": tuple(paths),
        "route": report,
        "anchors": tuple(anchors),
    }


def candidates(room, spec, identifier, method="wall-anchors", limit=64):
    check_identifier(identifier)
    if (
        method not in ("wall-anchors", "jittered-anchors")
        or type(limit) is not int
        or not 1 <= limit <= 256
    ):
        raise ValueError("Unknown furnishing candidate method or capacity")
    seed = int(hashlib.sha256(identifier.encode()).hexdigest()[:16], 16)
    rng = np.random.default_rng(seed)
    rows = []
    for a, b in sorted(
        edges(room), key=lambda e: (-np.linalg.norm(e[1] - e[0]), tuple(e[0]))
    ):
        vector = b - a
        length = float(np.linalg.norm(vector))
        tangent = vector / length
        side = spec.activity_depth_m + 0.05 if spec.kind == "meeting-group" else 0.06
        if length < spec.width_m + 2 * side:
            continue
        count = min(7, max(3, int(length / (spec.width_m + 0.5))))
        fractions = np.linspace(
            (spec.width_m / 2 + side) / length,
            1 - (spec.width_m / 2 + side) / length,
            count,
        )
        for fraction in fractions:
            shift = 0.0 if method == "wall-anchors" else float(rng.uniform(0.02, 0.35))
            origin = (
                a
                + vector * fraction
                + np.array([-tangent[1], tangent[0]])
                * (
                    (
                        spec.activity_depth_m + 0.05
                        if spec.kind == "meeting-group"
                        else 0.035
                    )
                    + shift
                )
            )
            row = placed(spec, origin, tangent, identifier + f"/candidate-{len(rows)}")
            if room.covers(row["footprint"]) and room.covers(row["activity"]):
                rows.append(row)
            if len(rows) >= limit:
                return rows
    return rows


def layout(
    room,
    portals,
    requests,
    identifier,
    reserved=(),
    method="wall-anchors",
    max_candidates=64,
    beam_width=1,
    objective="short-paths",
    *,
    placement_reserved=(),
):
    check_identifier(identifier)
    if not room.is_valid or room.geom_type != "Polygon" or room.is_empty:
        raise ValueError("Furnishing room must be a valid polygon")
    if len(requests) > 32 or not all(isinstance(r, Furnishing) for r in requests):
        raise ValueError("Furnishing request capacity exceeded")
    if type(beam_width) is not int or not 1 <= beam_width <= 8:
        raise ValueError("Furnishing beam capacity must lie in 1..8")
    if objective not in ("short-paths", "open-activity"):
        raise ValueError("Unknown furnishing layout objective")
    if (
        method not in ("wall-anchors", "jittered-anchors")
        or type(max_candidates) is not int
        or not 1 <= max_candidates <= 256
    ):
        raise ValueError("Unknown furnishing candidate method or capacity")
    rejected = []
    tested = 0
    placement_reserved = tuple(placement_reserved)
    proof = access(room, portals, [], reserved, placement_reserved=placement_reserved)
    states = [(0.0, (), [], proof)]
    for number, spec in enumerate(requests):
        choices = []
        pool = candidates(
            room, spec, identifier + f"/item-{number}", method, max_candidates
        )
        for _, _, items, _ in states:
            for item in pool:
                tested += 1
                try:
                    current = access(
                        room,
                        portals,
                        [*items, item],
                        reserved,
                        placement_reserved=placement_reserved,
                    )
                except ValueError as error:
                    rejected.append({"kind": spec.kind, "reason": str(error)})
                    continue
                item = item | {"id": identifier + f"/item-{number}"}
                proposed = [*items, item]
                cost = sum(path.length for path in current["paths"])
                if objective == "open-activity":
                    spacing = sum(
                        a["activity"].intersection(b["activity"]).area
                        for i, a in enumerate(proposed)
                        for b in proposed[:i]
                    )
                    cost = 0.15 * cost + 4.0 * spacing
                key = tuple(float(value) for row in proposed for value in row["centre"])
                choices.append((cost, key, proposed, current))
        if not choices:
            _, _, items, proof = states[0]
            return {
                "status": "incomplete",
                "items": tuple(items),
                "proof": proof,
                "tested": tested,
                "rejected": rejected,
                "missing": tuple(r.kind for r in requests[number:]),
                "reason": "No bounded candidate preserves every requested activity and portal",
            }
        states = sorted(choices, key=lambda x: x[:2])[:beam_width]
    _, _, items, proof = states[0]
    return {
        "status": "complete",
        "items": tuple(items),
        "proof": proof,
        "tested": tested,
        "rejected": rejected,
        "missing": (),
    }
