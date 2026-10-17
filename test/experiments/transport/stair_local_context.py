"""Conservative complete local terrain and whole-pier dependency neighbourhoods."""

import numpy as np
import shapely
from stair_local_assets import road_window


def require(result):
    if result.mesh is None:
        raise ValueError("local resident terrain query failed: " + result.reason)
    return result


def radius(view):
    bound = max(abs(v) for p in view.patches for v in p.bounds_xy_m)
    return float(4 * abs(np.spacing(np.float32(bound)))) + 1e-7


def neighbourhood(view, axes, contacts, *, max_faces=8192, method="index"):
    initial = require(view.nearby(axes, 40.0, max_faces=max_faces, method=method))
    keys = set(initial.face_keys)
    feet = [
        shapely.union_all(shapely.polygons(low.vertices[low.triangles, :2]))
        for low, _high in contacts
    ]
    completed, work = set(), [dict(initial.report)]
    guard = radius(view)
    for _iteration in range(129):
        result = require(view.faces(tuple(keys), max_faces=max_faces))
        region = shapely.union_all(
            shapely.polygons(result.mesh.vertices[result.mesh.triangles, :2])
        )
        hits = [
            i
            for i, foot in enumerate(feet)
            if i not in completed and region.intersects(foot)
        ]
        if not hits:
            auxiliary = require(
                view.nearby(region, 5.0, max_faces=max_faces, method=method)
            )
            keys.update(auxiliary.face_keys)
            result = require(view.faces(tuple(keys), max_faces=max_faces))
            return result, {
                "initial_query": work[0],
                "dependency_queries": work[1:],
                "auxiliary_ground_query": dict(auxiliary.report),
                "completed_pier_groups": sorted(completed),
                "context_faces": len(keys),
                "pier_query_radius_m": guard,
            }
        for i in hits:
            found = require(
                view.nearby(feet[i], guard, max_faces=max_faces, method=method)
            )
            keys.update(found.face_keys)
            work.append(dict(found.report))
            completed.add(i)
    raise ValueError("bounded local context pier closure exhausted")


def state_for(
    view, assets, piers, contacts, sources, recipe, *, max_faces=8192, method="index"
):
    axes = shapely.union_all([sources[k]["shape"] for k in recipe["source_ids"]])
    query, note = neighbourhood(
        view, axes, contacts, max_faces=max_faces, method=method
    )
    region = shapely.union_all(
        shapely.polygons(query.mesh.vertices[query.mesh.triangles, :2])
    )
    road, ground, keys, rows, road_note = road_window(
        assets, region, xy_guard_m=radius(view)
    )
    state = {
        "resident_profile_roots": True,
        "road": road,
        "road_ground": ground,
        "cached_module_rows": rows,
        "report": {"candidate_source_ids": keys, "after": {"source_ids": keys}},
        "plan": {
            "xy": query.mesh.vertices[:, :2],
            "faces": query.mesh.triangles,
            "ground": query.face_data["ground"],
        },
        "heights": query.mesh.vertices[:, 2],
        "piers": piers,
        "pier_contacts": contacts,
    }
    return state, query, axes, {"terrain": note, "roads": road_note}


def selected_keys(view, context, chosen, footprint, contacts):
    exact = require(view.nearby(footprint, 24.0, max_faces=2048))
    expected = set(exact.face_keys)
    present = set(context.face_keys)
    if not expected <= present:
        raise ValueError("initial local context omitted required terrain neighbours")
    for _iteration in range(129):
        before = len(expected)
        for low, _high in contacts:
            foot = shapely.union_all(shapely.polygons(low.vertices[low.triangles, :2]))
            group = view.nearby(foot, 0.0, max_faces=2048)
            if group.mesh is None:
                raise ValueError("whole old pier has no actual resident terrain")
            if expected.intersection(group.face_keys):
                expected.update(group.face_keys)
        if len(expected) > 2048 or not expected <= present:
            raise ValueError("local terrain dependency budget or completeness failure")
        if len(expected) == before:
            break
    else:
        raise ValueError("local terrain dependency closure exhausted")
    actual = {
        k for k, selected in zip(context.face_keys, chosen, strict=True) if selected
    }
    if actual != expected:
        raise ValueError("local planner changed exact whole-asset terrain selection")
    removals = {}
    for key, index in sorted(actual):
        removals.setdefault(key, []).append(index)
    return removals
