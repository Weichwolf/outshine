"""Two disjoint flat-ended C1 transitions share one projected height coordinate."""

import numpy as np
import shapely
from bridge_approach_recipe import height_field
from outshine_ref.contracts import Mesh
from outshine_ref.transport import (
    minimum_transition_length,
    plan_vertical_transition,
    sample_vertical_transition,
)
from overpass_clearance import vertical_clearance
from retained_surface import flat_domain
from shared_bridge_geometry import corridor

TOP_ROLES = ("deck", "approach_ground")


def extension_field(sources, approaches, root, old, lower, budget):
    field, west = height_field(approaches, *root, "parabolic")
    if field is None:
        raise ValueError(west["reason"])
    _domain, origin, axis, west_table, curve = field
    domain = shapely.union_all([corridor(s) for s in sources if s["role"] in TOP_ROLES])
    xy = shapely.get_coordinates(domain)
    station = (xy - origin) @ axis
    height = float(root[0].vertices[0, 2])
    flat = flat_domain(domain, budget=budget)
    probe = vertical_clearance(
        lower, Mesh(flat.vertices + [0, 0, height - 0.35], flat.triangles)
    )
    if not probe["valid"]:
        raise ValueError(probe["reason"])
    rise = max(0.0, 4.52 - probe["minimum_clearance_m"])
    if rise > 8:
        raise ValueError("eastern_height_change_budget")
    maximum = budget.max_stations - len(west_table[1:-1]) - 2
    if maximum < 2:
        raise ValueError("projected_station_budget")
    if rise == 0:
        joined = np.vstack(
            (
                [station.min() - 1, height],
                west_table[1:-1],
                [station.max() + 1, west["far_height_m"]],
            )
        )
        return (domain, origin, axis, joined, curve), {
            "west": west,
            "east": {"deck_rise_m": 0.0},
            "maximum_height_chord_bound_m": west["sampling"][
                "maximum_height_chord_bound_m"
            ],
            "analytic_c1_height_field": True,
            "mesh_continuity": "C0",
        }
    near = float(((old[0].vertices[:, :2] - origin) @ axis).min()) - 2
    length = minimum_transition_length(rise, 0.024, 0.002)
    if length > near - station.min() - 4:
        raise ValueError("insufficient_eastern_transition_space")
    east, note = plan_vertical_transition(
        height + rise, height, length, grade=0.024, second_derivative=0.002
    )
    if east is None:
        raise ValueError(note["reason"])
    table, sampling = sample_vertical_transition(east, max_vertices=maximum)
    if table is None:
        raise ValueError(sampling["reason"])
    first = near - length
    table[:, 0] += first
    joined = np.vstack(
        (
            [station.min() - 1, height + rise],
            table,
            west_table[1:-1],
            [station.max() + 1, west["far_height_m"]],
        )
    )
    note.update(
        transition_station_m=[first, near],
        deck_rise_m=rise,
        minimum_feasible_length_m=length,
        sampling=sampling,
        original_flat_clearance=probe,
        curve_breaks_m=east.breaks.tolist(),
        curve_coefficients=east.coefficients.tolist(),
    )
    return (domain, origin, axis, joined, curve), {
        "west": west,
        "east": note,
        "maximum_height_chord_bound_m": max(
            sampling["maximum_height_chord_bound_m"],
            west["sampling"]["maximum_height_chord_bound_m"],
        ),
        "analytic_c1_height_field": True,
        "mesh_continuity": "C0",
        "sampled_height_table": joined.tolist(),
    }


def field_certificate(mesh, field, profile):
    _domain, origin, axis, table, _curve = field
    station = (mesh.vertices[:, :2] - origin) @ axis
    expected = np.interp(station, table[:, 0], table[:, 1])
    vertex_error = float(abs(mesh.vertices[:, 2] - expected).max())
    ranges = station[mesh.triangles]
    cuts = np.searchsorted(table[:, 0], ranges.min(axis=1) + 1e-8, side="right")
    if np.any(ranges.max(axis=1) > table[np.minimum(cuts, len(table) - 1), 0] + 1e-8):
        raise ValueError("extended_face_straddles_height_chords")
    bound = profile["maximum_height_chord_bound_m"] + vertex_error + 1.0024e-7
    if bound > 0.01 + 1.0024e-7:
        raise ValueError("extended_face_height_error_budget")
    return {
        "whole_face_height_error_bound_m": bound,
        "each_face_within_one_height_chord": True,
        "maximum_table_vertex_error_m": vertex_error,
    }
