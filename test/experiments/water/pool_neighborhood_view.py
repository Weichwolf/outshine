"""One shared scientific depth camera for the original nine-page before/after."""

import numpy as np
from pool_contact_view import _basis, _section, _view


def display(body, neighborhood, vertices, faces, witness_indices, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    points = np.vstack(
        (
            neighborhood.vertices,
            vertices,
            body["vertices"],
            body["water_triangles"].reshape(-1, 3),
        )
    )
    basis = _basis()
    projected = points @ basis.T
    low, high = projected.min(axis=0), projected.max(axis=0)
    centre = ((low + high) / 2) @ basis
    span_m = max(high[0] - low[0], (high[1] - low[1]) * 1280 / 720) / 0.94
    figure, axes = plt.subplots(1, 3, figsize=(18, 6), constrained_layout=True)
    for axis, source, indices, title in (
        (
            axes[0],
            neighborhood.vertices,
            neighborhood.faces,
            "Original native end heights",
        ),
        (axes[1], vertices, faces, "Body + reconciled terrain reference"),
    ):
        axis.imshow(_view(body, source, indices, centre, span_m=span_m))
        axis.set(title=title, xlabel="pixel x", ylabel="pixel y")
    original = neighborhood.vertices[witness_indices]
    _section(axes[2], body, original, np.array([[0, 2, 3], [0, 3, 1]]), vertices, faces)
    axes[2].set_title("Original witness diagonal inside the qualified window")
    figure.suptitle(
        "1280×720 CPU depth views; actual original nine-page heights / WGS84 Double reference; no native packing or PBR admission"
    )
    figure.savefig(output / "pool-neighborhood.png", dpi=150)
    plt.close(figure)
