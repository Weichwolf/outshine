"""Reconcile two bounded patches through their shared original edge bindings."""

from fractions import Fraction
from itertools import pairwise

import numpy as np
import shapely
from pool_terrain_contact import contact_patch


def boundary_edges(faces):
    edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    return set(map(tuple, edges[counts == 1]))


def edge_profile(source, patch, edge):
    vertices, _, _, report = patch
    slots = {tuple(p[:2]): i for i, p in enumerate(vertices)}
    result = {
        Fraction(i): vertices[slots[tuple(source[node, :2])]]
        for i, node in enumerate(edge)
    }
    for port in report["work"]["exact_boundary_parameters"]:
        if tuple(port["input_edge"]) == edge:
            result[Fraction(port["parameter"])] = vertices[port["output_vertex"]]
    return result


def profile_gap(first, second):
    def height(profile, parameter):
        if parameter in profile:
            return profile[parameter][2]
        keys = sorted(profile)
        for a, b in pairwise(keys):
            if a < parameter < b:
                return profile[a][2] + float((parameter - a) / (b - a)) * (
                    profile[b][2] - profile[a][2]
                )
        raise ValueError("shared edge profile is incomplete")

    return max(
        abs(height(first, t) - height(second, t)) for t in first.keys() | second.keys()
    )


def reconcile_pair(body, source, first_faces, second_faces):
    """Use one shared source vertex frame; outer neighbors remain unqualified."""
    if not all(0 < len(faces) <= 64 for faces in (first_faces, second_faces)):
        raise ValueError("two bounded nonempty original patches required")
    domains = shapely.polygons(source[np.vstack((first_faces, second_faces)), :2])
    if not shapely.coverage_is_valid(domains):
        raise ValueError("two nonoverlapping conforming original patches required")
    shared = boundary_edges(first_faces) & boundary_edges(second_faces)
    if not shared:
        raise ValueError("two supplied patches must share original boundary edges")
    initial = [
        contact_patch(body, source, faces) for faces in (first_faces, second_faces)
    ]
    requests, before = [[], []], 0.0
    knots = 0
    for edge in sorted(shared):
        profiles = [edge_profile(source, patch, edge) for patch in initial]
        before = max(before, profile_gap(*profiles))
        shared_knots = profiles[0].keys() | profiles[1].keys()
        knots += sum(0 < t < 1 for t in shared_knots)
        for i, profile in enumerate(profiles):
            requests[i].extend(
                {"input_edge": list(edge), "parameter": str(t)}
                for t in sorted(shared_knots - profile.keys())
                if 0 < t < 1
            )
    updated = [
        contact_patch(body, source, faces, boundary_requests=needed)
        if needed
        else patch
        for patch, faces, needed in zip(initial, (first_faces, second_faces), requests)
    ]
    after = 0.0
    for edge in sorted(shared):
        first, second = [edge_profile(source, patch, edge) for patch in updated]
        if first.keys() != second.keys():
            raise ValueError("shared boundary subdivision remains unresolved")
        after = max(after, profile_gap(first, second))
        for t in first:
            if (
                first[t][:2].tobytes() != second[t][:2].tobytes()
                or first[t].astype(np.float32).tobytes()
                != second[t].astype(np.float32).tobytes()
            ):
                raise ValueError("shared boundary position words remain unresolved")
    if after > 1e-7:
        raise ValueError("shared boundary heights remain unresolved")
    return (
        initial,
        updated,
        {
            "shared_original_edges": len(shared),
            "shared_internal_knots": knots,
            "requested_missing_knots_per_side": list(map(len, requests)),
            "rebuilt_patches": sum(bool(needed) for needed in requests),
            "maximum_initial_seam_gap_m": float(before),
            "maximum_reconciled_seam_gap_m": float(after),
            "scope": "Two bounded patches in one supplied source frame; outer neighbor obligations and native terrain inputs remain open.",
        },
    )
