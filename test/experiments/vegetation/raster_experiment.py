"""Independent raster regressions and equal-input CPU cost comparisons."""

import time

import numpy as np
from group_cloud import make_chart, organ_statistics
from leaf_sheet import rasterize as reference
from raster_batch import rasterize as batched


def checks(source):
    rng = np.random.default_rng(4)
    cases = []
    for count in (3, 30, 300):
        vertices = rng.uniform(-1, 1, (count * 3, 3))
        faces = np.arange(count * 3).reshape(-1, 3)
        attributes = rng.normal(size=(count, 3, 6))
        cases.append((vertices, faces, attributes))
    vertices = np.asarray(((-0.8, -0.6, 0), (0.8, -0.6, 0), (0, 0.9, 0)))
    faces = np.asarray(((0, 1, 2), (0, 1, 2), (0, 0, 0)))
    cases.append(
        (vertices, faces, np.broadcast_to(np.arange(3)[:, None, None], (3, 3, 6)))
    )
    cases.append(
        (source["vertices"], source["faces"], source["normals"].reshape(-1, 3, 3))
    )
    for vertices, faces, attributes in cases:
        lower, upper = (
            vertices[:, :2].min(axis=0) - 0.1,
            vertices[:, :2].max(axis=0) + 0.1,
        )
        expected = reference(vertices, faces, attributes, lower, upper, 32)
        for budget in (1024, 4096, 262144):
            actual = batched(vertices, faces, attributes, lower, upper, 32, budget)
            np.testing.assert_array_equal(actual[0], expected[0])
            np.testing.assert_allclose(actual[1], expected[1], atol=2e-6)
    results = {}
    vertices, faces = source["vertices"], source["faces"]
    attributes = source["normals"].reshape(-1, 3, 3)
    lower, upper = vertices[:, :2].min(axis=0), vertices[:, :2].max(axis=0)
    for name, method in (("per_triangle", reference), ("bounded_batches", batched)):
        samples = []
        for _ in range(5):
            began = time.perf_counter()
            method(vertices, faces, attributes, lower, upper, 64)
            samples.append((time.perf_counter() - began) * 1000)
        results[name] = {"samples_ms": samples, "median_ms": float(np.median(samples))}
    # One real projected chart also exercises exact coverage and material parameters.
    import group_cloud

    normal = organ_statistics(source)[1][0]
    group_cloud.rasterize = reference
    expected = make_chart(source, np.arange(len(source["ids"])), normal, 32)
    group_cloud.rasterize = batched
    actual = make_chart(source, np.arange(len(source["ids"])), normal, 32)
    np.testing.assert_allclose(actual.colour, expected.colour, atol=2e-6)
    np.testing.assert_allclose(actual.normal, expected.normal, atol=2e-6)
    return {
        "cases": len(cases) * 3 + 1,
        "cost": results,
        "scope": "same group vertices, faces and attributes, 64 by 64 raster; five local CPU samples, setup excluded, no GPU or p95 claim",
    }
