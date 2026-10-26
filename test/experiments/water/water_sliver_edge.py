"""Explicit single-edge counterfactual, not an automatic mesh repair (#935)."""

from dataclasses import dataclass
from fractions import Fraction
from math import nextafter

import numpy as np
from outshine_ref.contracts import Mesh


@dataclass(frozen=True)
class Contraction:
    mesh: Mesh
    rows: np.ndarray
    primary: np.ndarray
    vertices: np.ndarray
    faces: np.ndarray
    row_indices: np.ndarray
    removed_faces: np.ndarray
    report: dict


def exact_area(points):
    """Signed area of the exact binary64 coordinates, without a GEOS overlay."""
    p = [[Fraction(float(x)) for x in point[:2]] for point in points]
    return sum(a[0] * b[1] - a[1] * b[0] for a, b in zip(p, p[1:] + p[:1])) / 2


def contract_edge(mesh, rows, primary, pair, *, maximum_shift_m, protected=()):
    """Caller explicitly proposes one edge and its maximum XY perturbation.

    Keeps the lower existing index, removes only faces made index-degenerate,
    and maps every surviving face/constraint back to its original input row.
    No other small-area face is removed. Admission of topology/source/contact
    changes belongs to the caller; this returns a measured counterfactual only.
    """
    pair = np.asarray(pair)
    if (
        pair.shape != (2,)
        or pair.dtype.kind not in "iu"
        or len(np.unique(pair)) != 2
        or pair.min() < 0
        or pair.max() >= len(mesh.vertices)
        or not np.isfinite(maximum_shift_m)
        or maximum_shift_m < 0
    ):
        raise ValueError("one existing edge and explicit finite shift bound required")
    keep, remove = sorted(map(int, pair))
    if keep in protected or remove in protected:
        raise ValueError("protected vertex contact requires a separate decision")
    incident = np.any(mesh.triangles == keep, axis=1) & np.any(
        mesh.triangles == remove, axis=1
    )
    if not incident.any():
        raise ValueError("the proposed pair is not a mesh edge")
    displacement = float(
        np.linalg.norm(mesh.vertices[keep, :2] - mesh.vertices[remove, :2])
    )
    if displacement > maximum_shift_m:
        raise ValueError("proposed contraction exceeds the explicit XY shift bound")
    replacement = np.arange(len(mesh.vertices))
    replacement[remove] = keep
    mapped = replacement[mesh.triangles]
    retained = np.all(np.diff(np.sort(mapped, axis=1), axis=1) != 0, axis=1)
    original_faces = np.flatnonzero(retained)
    removed_faces = np.flatnonzero(~retained)
    vertices, inverse = np.unique(mapped[retained], return_inverse=True)
    result = Mesh(mesh.vertices[vertices], inverse.reshape(-1, 3))
    face_map = np.full(len(mesh.triangles), -1)
    face_map[retained] = np.arange(len(original_faces))
    rows = np.asarray(rows)
    selected = np.flatnonzero(retained[rows[:, 0].astype(int)])
    new_rows = rows[selected].copy()
    new_rows[:, 0] = face_map[new_rows[:, 0].astype(int)]
    row_map = np.full(len(rows), -1)
    row_map[selected] = np.arange(len(selected))
    new_primary = row_map[np.asarray(primary)[retained]]
    if np.any(new_primary < 0):
        raise ValueError("surviving primary row was lost")
    old_triangles = mesh.vertices[mesh.triangles, :2]
    new_triangles = result.vertices[result.triangles, :2]
    changed = np.flatnonzero(np.any(mesh.triangles == remove, axis=1))
    swept = Fraction(0)
    for face in changed:
        opposite = old_triangles[face][mesh.triangles[face] != remove]
        for fixed in opposite:
            swept += abs(
                exact_area([mesh.vertices[remove, :2], mesh.vertices[keep, :2], fixed])
            )
    report = {
        "pair": [keep, remove],
        "maximum_shift_m": displacement,
        "requested_shift_bound_m": maximum_shift_m,
        "changed_original_faces": changed.tolist(),
        "removed_original_faces": removed_faces.tolist(),
        "removed_constraint_rows": int(len(rows) - len(selected)),
        "removed_face_area_m2": float(
            sum(abs(exact_area(t)) for t in old_triangles[~retained])
        ),
        "swept_symmetric_difference_bound_m2": nextafter(float(swept), float("inf")),
        "swept_bound_exact": str(swept),
        "positive_remaining_exact_areas": all(exact_area(t) > 0 for t in new_triangles),
        "input_vertices": len(mesh.vertices),
        "input_faces": len(mesh.triangles),
        "output_vertices": len(result.vertices),
        "output_faces": len(result.triangles),
        "scanned_face_indices": int(mesh.triangles.size),
        "output_array_bytes": sum(
            a.nbytes
            for a in (
                result.vertices,
                result.triangles,
                new_rows,
                new_primary,
                vertices,
                original_faces,
                selected,
                removed_faces,
            )
        ),
    }
    return Contraction(
        result,
        new_rows,
        new_primary,
        vertices,
        original_faces,
        selected,
        removed_faces,
        report,
    )
