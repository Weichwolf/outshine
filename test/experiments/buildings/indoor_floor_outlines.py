"""Small explicit outer-envelope adaptation to fixed source pedestrian demand."""

from dataclasses import replace

import numpy as np
import shapely
from indoor_floor_plan import floor_domain


def adapt_outlines(parts, constraints, limits):
    if (
        len(parts) > limits.max_source_parts
        or len(constraints) > limits.max_constraints
    ):
        raise ValueError("Outline adaptation source budget exceeded")
    if len({p.frame for p in parts} | {r.frame for r in constraints}) != 1:
        raise ValueError("One bound source frame required before outline adaptation")
    parts = tuple(sorted(parts, key=lambda p: p.identifier))
    tree = shapely.STRtree([p.polygon for p in parts])
    additions, sources, budgets = {}, {}, {}
    work = {"static_ground_constraints": 0, "candidate_parts": 0, "accepted_demands": 0}
    for request in sorted(constraints, key=lambda r: r.identifier):
        if (
            request.purpose != "flat_indoor_walkway"
            or type(request.level) is not int
            or request.level != 0
        ):
            continue
        work["static_ground_constraints"] += 1
        nearby = tree.query(
            request.corridor.buffer(limits.outline_adjustment_m), predicate="intersects"
        )
        work["candidate_parts"] += len(nearby)
        if len(nearby) > limits.max_parts:
            continue
        if floor_domain(tuple(parts[int(i)] for i in nearby), 0, limits).covers(
            request.corridor
        ):
            continue
        demand = request.corridor.buffer(limits.outline_clearance_m, join_style="mitre")
        options, shared = [], []
        for index in map(int, nearby):
            part = parts[index]
            if (
                part.lower_m != 0
                or part.upper_m < limits.headroom_m + 2 * limits.slab_m
            ):
                continue
            if (
                part.polygon.covers(request.corridor)
                or part.polygon.intersection(request.corridor).area == 0
            ):
                continue
            step = part.source.get("coordinate_step_m", 0.0)
            if type(step) not in (int, float) or not np.isfinite(step) or step <= 0:
                continue
            budget = min(limits.outline_adjustment_m, limits.outline_grid_steps * step)
            if not np.isfinite(budget) or budget <= 0:
                continue
            exterior = shapely.Polygon(part.polygon.exterior)
            scale = max(1.0, float(abs(shapely.get_coordinates(exterior)).max()))
            radius = budget - 128 * np.spacing(scale)
            if radius <= 0:
                continue
            fringe = exterior.buffer(radius).difference(exterior)
            allowed = shapely.union(part.polygon, fringe)
            shared.append((index, budget, allowed))
            if not allowed.covers(demand):
                continue
            extra = demand.difference(part.polygon)
            options.append((float(extra.area), part.identifier, index, budget))
        if options:
            _, _, index, budget = min(options)
            selected = [(index, budget, demand)]
        elif shared and shapely.union_all([a for _, _, a in shared]).covers(demand):
            selected = [
                (index, budget, shapely.intersection(demand, allowed))
                for index, budget, allowed in shared
            ]
        else:
            continue
        applied = False
        for index, budget, piece in selected:
            if piece.difference(parts[index].polygon).area == 0:
                continue
            additions.setdefault(index, []).append(piece)
            sources.setdefault(index, []).append(request.identifier)
            budgets[index] = budget
            applied = True
        if not applied:
            continue
        work["accepted_demands"] += 1
    adapted, records = [], []
    for index, part in enumerate(parts):
        if index not in additions:
            adapted.append(part)
            continue
        polygon = shapely.union_all([part.polygon, *additions[index]])
        if (
            polygon.geom_type != "Polygon"
            or not polygon.is_valid
            or int(shapely.get_num_coordinates(polygon)) > limits.max_vertices
        ):
            adapted.append(part)
            continue
        added = polygon.difference(part.polygon)
        distance = shapely.distance(
            shapely.points(shapely.get_coordinates(added)), part.polygon
        )
        if distance.max(initial=0) > budgets[index] or not shapely.Polygon(
            part.polygon.exterior
        ).buffer(budgets[index]).covers(polygon):
            adapted.append(part)
            continue
        adapted.append(replace(part, polygon=polygon))
        records.append(
            {
                "source_id": part.identifier,
                "demand_source_ids": sources[index],
                "maximum_adjustment_budget_m": budgets[index],
                "added_area_m2": float(added.area),
                "maximum_added_vertex_distance_m": float(distance.max(initial=0)),
                "original_wkb_hex": part.polygon.wkb_hex,
                "adjusted_wkb_hex": polygon.wkb_hex,
            }
        )
    return tuple(adapted), {
        "work": work,
        "adjustments": records,
        "scope": "Explicit procedural outer-footprint change before building construction, bounded by a metric cap and tile-centre grid-step fraction. Original source volume/attributes retained; no courtyard filling, containment tolerance change, statics or external World clearance certificate.",
    }
