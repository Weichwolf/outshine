"""Explicit pad extensions at original nodes; immutable old bodies keep their frame."""

import numpy as np
import shapely
from stair_cached_box_grids import outward_box
from stair_cycle_contacts import interior_profile_xy, profile
from stair_module_node_heights import plan_heights


def whole_sections(row):
    xy = np.asarray(row.get("surface_axis_xy_m", row["xy"]))
    selected = dict(row, xy=xy)
    lines = [profile(selected, 0)]
    lines.extend(interior_profile_xy(selected, i) for i in range(1, len(xy) - 1))
    lines.append(profile(selected, -1))
    return [shapely.LineString(p[[0, -1]]) for p in lines]


def box_plans(rows, nodes, line, contacts, *, include_interiors, cached_grids=None):
    cached = [line, *[c["profile"] for c in contacts]]
    plans = plan_heights(
        rows, nodes, [l[1, :2] for l in cached], include_interiors=include_interiors
    )
    boxes = {tuple(p["mvt_xy"]): p["shape"] for p in plans}
    for node in nodes:
        old = [l for l in cached if np.array_equal(l[1, :2], node["xy"])]
        if not old:
            continue
        reach = (
            max(
                max(
                    float(np.linalg.norm(l[:, :2] - node["xy"], axis=1).max())
                    for l in old
                ),
                max(rows[i]["width_m"] / 2 for i in node["incident"]),
            )
            + 0.75
        )
        x, y = node["xy"]
        boxes[tuple(node["mvt_xy"])] = shapely.box(
            x - reach, y - reach, x + reach, y + reach
        )
    for node, grid in (cached_grids or {}).items():
        if node not in boxes:
            raise ValueError("cached grid anchor missing from original shared nodes")
        boxes[node] = outward_box(boxes[node], grid)
    return boxes


def add_boxes(rows, nodes, line, contacts, *, include_interiors, cached_grids=None):
    boxes = box_plans(
        rows,
        nodes,
        line,
        contacts,
        include_interiors=include_interiors,
        cached_grids=cached_grids,
    )
    for node in nodes:
        shape = boxes.get(tuple(node["mvt_xy"]))
        if shape is not None:
            for owner in node["incident"]:
                rows[owner].setdefault("junction_pads_xy_m", []).append(
                    np.array(shape.exterior.coords[:-1])
                )


def common_pad(
    data, nodes, rows, index, old_caps, old_lines, *, include_interiors, constructed=()
):
    from stair_landing_pads import node_profiles

    node = nodes[index]
    profiles = [
        p for _, _, p in node_profiles(rows, node, include_interiors=include_interiors)
    ]
    hull = shapely.MultiPoint(np.vstack([*profiles, *old_lines])).convex_hull
    vertices, faces = data["surface_vertices"], data["surface_faces"]
    incident = np.any(data["owners"][:, node["incident"]], axis=1)
    own_faces = faces[incident]
    extra = [
        (c["cap"].vertices, c["cap"].triangles, c["mask"].intersection(hull))
        for c in constructed
        if c["owner"] in node["incident"] and c["mask"].intersection(hull).area > 1e-7
    ]
    covered = shapely.union_all(
        [
            shapely.union_all(shapely.polygons(vertices[own_faces, :2])),
            *[
                shapely.union_all(shapely.polygons(c.vertices[c.triangles, :2]))
                for c in old_caps
            ],
            *[
                shapely.union_all(shapely.polygons(v[f, :2])).intersection(mask)
                for v, f, mask in extra
            ],
        ]
    )
    if hull.geom_type != "Polygon" or hull.difference(covered).area > 1e-7:
        raise ValueError("constructed common original-node pad is incomplete")
    return [(vertices, own_faces, hull), *extra]


def verify_masks(rows, nodes, original, masks, line, contacts):
    from stair_junction_layout import landings
    from stair_module_roots import profile_masks

    _, _, expected = landings(rows, original, "width", 0.75, nodes)
    expected = profile_masks(nodes, expected, line, contacts)
    if len(masks) != len(expected) or any(
        not actual.equals_exact(wanted, 0, normalize=True)
        for actual, wanted in zip(masks, expected, strict=True)
    ):
        raise ValueError("saved shared-box masks differ from full original profiles")
