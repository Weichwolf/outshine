"""Independent chart/partition and barycentric field-evaluation equivalence checks."""

import numpy as np
from bark_layers import bark_layers


def check_layers(axes, mesh, dense, indexed):
    count = len(axes)
    weights = np.column_stack([indexed[f"bark_weight_{i}"] for i in range(count)])
    assert np.isfinite(weights).all() and (weights >= 0).all()
    np.testing.assert_allclose(weights.sum(axis=1), 1, rtol=0, atol=1e-15)
    charts = np.stack([indexed[f"bark_chart_{i}"] for i in range(count)], axis=1)
    radii = np.column_stack([indexed[f"bark_radius_{i}"] for i in range(count)])
    np.testing.assert_allclose(
        np.sum(weights[..., None] * charts, axis=1),
        mesh["bark_coordinates"],
        rtol=0,
        atol=1e-14,
    )
    np.testing.assert_allclose(
        np.sum(weights * radii, axis=1), mesh["bark_radius"], rtol=0, atol=1e-14
    )
    relevant = 0
    for i in range(count):
        np.testing.assert_array_equal(
            indexed[f"bark_weight_{i}"], dense[f"bark_weight_{i}"]
        )
        active_faces = (weights[:, i] > 0)[mesh["faces"]].any(axis=1)
        corners = mesh["faces"][active_faces].ravel()
        relevant += len(corners)
        for prefix in ("chart", "radius"):
            np.testing.assert_array_equal(
                indexed[f"bark_{prefix}_{i}"][corners],
                dense[f"bark_{prefix}_{i}"][corners],
            )
    # A nonlinear independent scalar field samples inside actual triangles, not just vertices.
    rng = np.random.default_rng(17)
    faces = mesh["faces"][rng.integers(0, len(mesh["faces"]), 200)]
    bary = rng.dirichlet((1, 1, 1), len(faces))
    outputs = []
    for source in (dense, indexed):
        value = np.zeros(len(faces))
        for i in range(count):
            w = np.sum(source[f"bark_weight_{i}"][faces] * bary, axis=1)
            q = np.sum(source[f"bark_chart_{i}"][faces] * bary[..., None], axis=1)
            value += w * (
                np.sin(q[:, 0] * 850 + q[:, 2] * 32) + 0.2 * np.cos(q[:, 1] * 500)
            )
        outputs.append(value)
    np.testing.assert_array_equal(*outputs)
    assert bark_layers(axes, mesh, max_layers=1) is None
    return {
        "layers": count,
        "sum_one_max_error": float(abs(weights.sum(axis=1) - 1).max()),
        "relevant_triangle_corners": relevant,
        "interior_samples_equal": len(faces),
        "legacy_linear_fields_preserved": True,
    }
