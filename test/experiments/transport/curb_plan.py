"""Procedural outside traffic edge bands with explicit source/port/crossing exclusions."""

import numpy as np
import shapely

URBAN = {"primary", "secondary", "tertiary", "minor", "service"}
WIDTH_M = 0.18
HEIGHT_M = 0.12


def source_masks(graphs, raw, used, old):
    eligible, owners, other, ports = [], [], [], []
    for graph in graphs:
        active = np.array([any(s in used for s in ids) for ids in graph["sources"]])
        for edge in np.flatnonzero(active):
            ids = [s for s in graph["sources"][edge] if s in used]
            properties = [raw[s]["properties"] for s in ids]
            yes = all(
                p.get("class") in URBAN and p.get("surface") == "paved"
                for p in properties
            )
            line = graph["lines"][edge]
            half = float(graph["half_widths"][edge])
            if yes:
                eligible.append(line.buffer(half + 2.2, quad_segs=4))
                owners.append(ids)
            else:
                other.append(line.buffer(half + 0.5, quad_segs=4))
        degree = np.bincount(
            graph["edges"][active].ravel(), minlength=len(graph["vertices"])
        )
        for edge in np.flatnonzero(active):
            radius = float(graph["half_widths"][edge]) + WIDTH_M + 0.5
            for node in graph["edges"][edge]:
                if degree[node] == 1:
                    ports.append(
                        shapely.Point(graph["vertices"][node]).buffer(
                            radius, quad_segs=4
                        )
                    )
    for source in old:
        if source["role"] != "ground":
            continue
        p = source["properties"]
        line = shapely.LineString(source["xy"])
        half = source["width_m"] / 2
        if p.get("class") in URBAN and p.get("surface") == "paved":
            eligible.append(line.buffer(half + 2.2, quad_segs=4))
            owners.append([source["source_id"]])
        else:
            other.append(line.buffer(half + 0.5, quad_segs=4))
        ports.extend(
            shapely.Point(xy).buffer(half + WIDTH_M + 0.5, quad_segs=4)
            for xy in np.asarray(source["xy"])[[0, -1]]
        )
    if not 1 <= len(eligible) <= 8192:
        raise ValueError("bounded used urban curb source bands required")
    return eligible, owners, shapely.union_all(other), shapely.union_all(ports)


def select(traffic, world, eligible, owners, other, ports, obstacles):
    if (
        max(shapely.get_num_coordinates(traffic), shapely.get_num_coordinates(world))
        > 262144
    ):
        raise ValueError("curb world/traffic boundary budget")
    band = traffic.buffer(WIDTH_M, quad_segs=4).difference(traffic)
    chosen = band.intersection(shapely.union_all(eligible))
    excluded = shapely.union_all([other, ports, obstacles])
    chosen = chosen.difference(excluded).intersection(world.buffer(-0.01))
    parts = list(shapely.get_parts(chosen))
    if len(parts) > 8192 or shapely.get_num_coordinates(chosen) > 1048576:
        raise ValueError("curb candidate polygon/coordinate budget")
    parts.sort(key=lambda p: (-p.area, *p.bounds))
    tree = shapely.STRtree(eligible)
    rows = []
    for index, shape in enumerate(parts):
        reason = ""
        if shape.geom_type != "Polygon" or not shape.is_valid:
            reason = "invalid_curb_candidate"
        elif shape.area < 0.01:
            reason = "curb_candidate_below_0.01_m2"
        elif shapely.get_num_coordinates(shape) > 1024:
            reason = "curb_footprint_coordinate_budget"
        hits = tree.query(shape, predicate="intersects")
        ids = sorted({s for hit in hits for s in owners[hit]})
        rows.append(
            {
                "candidate": index,
                "shape": shape,
                "sources": ids,
                "area_m2": shape.area,
                "reason": reason,
            }
        )
    return (
        rows,
        excluded,
        {
            "nominal_maximum_width_xy_m": WIDTH_M,
            "height_vertical_m": HEIGHT_M,
            "full_outside_band_area_m2": band.area,
            "source_selected_area_m2": sum(p.area for p in parts),
            "candidates": len(parts),
            "eligible_source_bands": len(eligible),
        },
    )
