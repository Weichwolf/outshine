"""Whole-face bounds and actual finite road/soil/port quality."""

import numpy as np

from ._world import approximation_bounds, finite_ports, normal_gradients


def quality(field, coefficients, mesh, roles, *, refinement_reserve=1.0):
    errors, maximum = approximation_bounds(
        field, coefficients, mesh["xy"], mesh["faces"], mesh["parts"], mesh["frames"]
    )
    gradients = normal_gradients(mesh["xy"], mesh["faces"], mesh["heights"])
    normalized = np.linalg.norm(
        np.einsum("nki,ni->nk", mesh["frames"], gradients), axis=1
    )
    bad_ports, contacts, hard_ports = finite_ports(
        mesh["xy"],
        mesh["faces"],
        mesh["parts"],
        mesh["heights"],
        mesh["port_roots"],
        roles,
        mark_limit=0.005 * refinement_reserve,
        include_hard=True,
    )
    road = mesh["parts"] >= 0
    hard = (errors > 0.01) | (road & (normalized > 1.0000001))
    hard[hard_ports] = True
    marked = (errors > 0.01 * refinement_reserve) | (road & (normalized > 1.0000001))
    marked[bad_ports] = True
    note = {
        "maximum_height_chord_bound_m": float(errors.max()),
        "maximum_normalized_continuous_grade": maximum,
        "maximum_normalized_finite_grade": float(normalized.max()),
        "maximum_normalized_finite_road_grade": float(normalized[road].max(initial=0)),
        "maximum_normalized_soil_grade": float(normalized[~road].max(initial=0)),
        "bad_finite_road_faces": int(np.count_nonzero(road & (normalized > 1.0000001))),
        "bad_road_faces": int(np.count_nonzero(hard)),
        "refinement_target_faces": int(np.count_nonzero(marked)),
        "refinement_reserve": refinement_reserve,
        "bad_soil_faces": int(np.count_nonzero(~road & (normalized > 1.0000001))),
        "finite_contacts": contacts,
    }
    note["geometry_complete"] = not hard.any()
    note["whole_world_valid"] = bool(
        normalized.max() <= 1.0000001 and maximum <= 1.0000001
    )
    return marked, note, errors
