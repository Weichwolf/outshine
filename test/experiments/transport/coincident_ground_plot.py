"""Actual unchanged source bands and complete affine ground patch at physical scale."""

import numpy as np


def render(result, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    source, mesh = result["source"], result["mesh"]
    figure = plt.figure(figsize=(13, 6), layout="constrained")
    plan = figure.add_subplot(1, 2, 1)
    sections = result["sections"]
    bands = sections.reshape(-1, 4, 2)[:, [0, 1, 3, 2]]
    plan.add_collection(
        PolyCollection(bands, facecolors="lightsteelblue", edgecolors="navy", alpha=0.4)
    )
    local = dict(zip(source.node_ids, result["axes"], strict=True))
    for i, member in enumerate(source.members):
        a, b = (local[node] for node in member.nodes)
        shift = np.array([0.0, 0.07 * (i - 2.5)])
        plan.annotate(
            "",
            xy=b + shift,
            xytext=a + shift,
            arrowprops={"arrowstyle": "->", "linewidth": 1, "color": "black"},
        )
    plan.scatter(*result["axes"].T, color="black", s=20)
    plan.autoscale()
    plan.set_aspect("equal")
    plan.set(
        xlabel="east m",
        ylabel="north m",
        title="Original six directed spans on two whole 3.5m bands\nPrevious multi-loop/strip paths reject; arrows offset only for legibility",
    )
    view = figure.add_subplot(1, 2, 2, projection="3d")
    xyz = mesh.vertices.copy()
    datum = float(xyz[:, 2].mean())
    xyz[:, 2] -= datum
    view.plot_trisurf(
        *xyz.T,
        triangles=mesh.triangles,
        color="steelblue",
        edgecolor="black",
        linewidth=1,
        alpha=0.85,
    )
    observations = result["observations"].copy()
    observations[:, 2] -= datum
    view.scatter(
        *observations.T, color="crimson", s=18, label="12 original DEM observations"
    )
    for band in sections:
        for section in band:
            height = (
                result["field"].intercept
                + (section - result["field"].origin) @ result["field"].gradient
            )
            view.plot(*section.T, height - datum, color="navy", linewidth=2)
    complete = np.vstack((xyz, observations))
    size = np.maximum(np.ptp(complete, axis=0), 1e-6)
    centre = (complete.min(axis=0) + complete.max(axis=0)) / 2
    for i, set_limit in enumerate((view.set_xlim, view.set_ylim, view.set_zlim)):
        set_limit(centre[i] - 0.55 * size[i], centre[i] + 0.55 * size[i])
    view.set_box_aspect(size)
    view.set_zticks([0.0])
    view.set(
        xlabel="east m",
        ylabel="north m",
        zlabel=f"height minus {datum:.3f}m",
        title="One complete affine physical patch:6vertices/4triangles\nNo double coverage; all six original owners retained",
    )
    view.legend(fontsize=7)
    figure.suptitle(
        "Actual bounded ground geometry, physical aspect\nNo original-end/Source/World/routing/vehicle-turn/device admission"
    )
    figure.savefig(output / "geometry.png", dpi=150)
    plt.close(figure)
