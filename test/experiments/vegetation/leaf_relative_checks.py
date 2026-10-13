"""Independent definition and degeneracies for relative-neighbor influences."""

import numpy as np
from leaf_relative_neighbors import relative_neighbors


def scalar_neighbors(nodes, source):
    result = []
    for v, point in enumerate(nodes):
        radius2 = sum((point[j] - source[j]) ** 2 for j in range(2))
        blocked = False
        for other in nodes:
            source2 = sum((other[j] - source[j]) ** 2 for j in range(2))
            node2 = sum((other[j] - point[j]) ** 2 for j in range(2))
            if max(source2, node2) < radius2:
                blocked = True
                break
        if not blocked:
            result.append(v)
    return np.asarray(result, dtype=int)


def check_neighbors():
    rng = np.random.default_rng(51)
    cases = [
        (np.array([[0.0, 0]]), np.array([[0.0, 0], [1.0, 1]])),
        (np.array([[-1.0, 0], [1.0, 0]]), np.array([[0.0, 0]])),
        (np.array([[0.0, 0], [0.0, 0], [1.0, 0]]), np.array([[0.0, 0]])),
        (np.array([[1.0, 2], [2.0, 1], [-1.0, 2], [-2.0, 1]]), np.zeros((1, 2))),
        (np.column_stack((np.arange(-8.0, 9), np.zeros(17))), rng.normal(size=(8, 2))),
    ]
    ring = np.array(
        [
            (x, y)
            for x in range(-34, 35)
            for y in range(-34, 35)
            if x * x + y * y == 1105
        ],
        dtype=float,
    )
    tied, _ = relative_neighbors(ring, np.zeros((1, 2)))
    assert len(tied[0]) == len(ring) and len(ring) > 8
    cases.append((ring, np.zeros((1, 2))))
    for _ in range(20):
        nodes, sources = rng.normal(size=(32, 2)), rng.normal(size=(8, 2))
        cases.append((nodes, sources))
    checked = 0
    for nodes, sources in cases:
        expected = [scalar_neighbors(nodes, s) for s in sources]
        for method in ("dense", "sectors"):
            result, _ = relative_neighbors(nodes, sources, method)
            assert all(np.array_equal(a, b) for a, b in zip(expected, result))
        checked += len(sources)
    far_nodes = np.vstack(
        (np.column_stack((-1 - np.arange(9) / 100, np.zeros(9))), [10.0, 0])
    )
    far, _ = relative_neighbors(far_nodes, np.zeros((1, 2)))
    assert np.array_equal(far[0], [0, 9])
    assert 9 not in np.argsort(np.sum(far_nodes**2, axis=1))[:8]
    for scale in (1e-6, 1, 1e6):
        transformed = far_nodes * scale + [3 * scale, 2 * scale]
        result, _ = relative_neighbors(transformed, np.array([[3 * scale, 2 * scale]]))
        assert np.array_equal(result[0], far[0])
    for method in ("dense", "sectors"):
        empty, _ = relative_neighbors(far_nodes, np.empty((0, 2)), method)
        assert empty == []
    for nodes, sources in (
        (np.empty((0, 2)), np.zeros((1, 2))),
        (np.zeros((4097, 2)), np.zeros((1, 2))),
        (np.zeros((1, 2)), np.zeros((4097, 2))),
        (np.array([[np.nan, 0]]), np.zeros((1, 2))),
    ):
        try:
            relative_neighbors(nodes, sources)
        except ValueError:
            continue
        raise AssertionError("invalid neighborhood request accepted")
    line = np.column_stack((np.arange(1.0, 4097), np.zeros(4096)))
    boundary, work = relative_neighbors(line, np.zeros((1, 2)))
    assert np.array_equal(boundary[0], [0]) and work["blocker_pairs"] == 4096
    try:
        relative_neighbors(line, np.zeros((36, 2)), "dense")
    except ValueError as error:
        assert "pair budget" in str(error)
    else:
        raise AssertionError("dense pair budget was relaxed")
    return {
        "capacity_4096_and_dense_budget": True,
        "scalar_sources": checked,
        "equal_radius_neighbors": len(ring),
        "ties_duplicates_collinear_and_coincident": True,
        "scale_translation_checks": 3,
        "fixed_eight_nearest_counterexample": {
            "nodes": far_nodes.tolist(),
            "source": [0, 0],
            "neighbors": far[0].tolist(),
            "missed_by_eight_nearest": 9,
        },
    }
