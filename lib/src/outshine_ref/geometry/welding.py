"""Bounded numeric vertex canonicalization; topology remains the caller's contract."""

from numbers import Real

import numpy as np


def weld_vertices(
    vertices: np.ndarray,
    tolerance_m: float,
    *,
    max_vertices: int = 65536,
    max_pairs: int = 65536,
) -> tuple[np.ndarray, np.ndarray, dict[str, int | float]]:
    """Return owned read-only first vertices, labels and actual displacement/work.

    Count radius candidates before allocating their array. Connected components
    retain their first supplied vertex only if every member lies within the
    displacement bound. A long transitive chain raises; it is never silently
    compressed. This is numeric canonicalization, not a topology/mesh validator.
    """
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    from scipy.spatial import cKDTree

    for value, minimum in ((max_vertices, 1), (max_pairs, 0)):
        if type(value) is not int or not minimum <= value <= 1048576:
            raise ValueError("bounded integer vertex and pair capacities required")
    if not hasattr(vertices, "__len__") or not 1 <= len(vertices) <= max_vertices:
        raise ValueError("bounded nonempty vertex input required")
    raw = np.asarray(vertices)
    if raw.ndim != 2 or raw.shape[1] not in (2, 3) or raw.dtype.kind not in "ifu":
        raise ValueError("real Nx2 or Nx3 local vertices required")
    points = np.asarray(raw, dtype=float)
    if (
        points.ndim != 2
        or points.shape[1] not in (2, 3)
        or not 1 <= len(points) <= max_vertices
        or not np.isfinite(points).all()
        or abs(points).max() > 1e9
    ):
        raise ValueError("bounded nonempty finite Nx2 or Nx3 local vertices required")
    if (
        isinstance(tolerance_m, (bool, np.bool_))
        or not isinstance(tolerance_m, Real)
        or not np.isfinite(tolerance_m)
        or not 1e-12 <= tolerance_m <= 1
    ):
        raise ValueError(
            "positive finite numeric displacement bound at most one metre required"
        )
    tree = cKDTree(points)
    count = (int(tree.count_neighbors(tree, tolerance_m)) - len(points)) // 2
    if count > max_pairs:
        raise ValueError("vertex weld pair budget exhausted")
    labels = np.arange(len(points))
    first = labels
    if count:
        pairs = tree.query_pairs(tolerance_m, output_type="ndarray")
        graph = coo_matrix(
            (np.ones(count), pairs.T), shape=(len(points), len(points))
        ).tocsr()
        _, labels = connected_components(graph, directed=False)
        first = np.full(int(labels.max()) + 1, len(points), dtype=int)
        np.minimum.at(first, labels, np.arange(len(points)))
    result = points[first]
    drift = float(np.linalg.norm(points - result[labels], axis=1).max(initial=0))
    if drift > tolerance_m:
        raise ValueError("vertex welding exceeds its numeric displacement bound")
    labels = labels.copy()
    result.flags.writeable = labels.flags.writeable = False
    return (
        result,
        labels,
        {
            "input_vertices": len(points),
            "output_vertices": len(result),
            "candidate_pairs": count,
            "maximum_displacement_m": drift,
            "owned_output_bytes": result.nbytes + labels.nbytes,
            "explicit_indexed_copy_bytes": points.nbytes,
            "explicit_difference_bytes": points.nbytes,
            "displacement_result_bytes": len(points) * 8,
        },
    )
