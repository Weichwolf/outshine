"""Inspect actual source topology, generated width and the complete finite surface."""

import numpy as np


def plot(output, inputs, mesh, report, *, title):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    geometry = report["geometry"]
    points = np.array(geometry["source_nodes_EN"])
    node_ids = list(geometry["source_node_ids"])
    parts = inputs.face_parts[report["surface_source_faces"]]
    figure = plt.figure(figsize=(14, 5), layout="constrained")
    axis = figure.add_subplot(121)
    axis.add_collection(
        PolyCollection(
            mesh.vertices[mesh.triangles, :2],
            array=parts,
            cmap="tab20",
            edgecolors=".45",
            linewidths=0.5,
        )
    )
    for a, b in geometry["source_edges_local"]:
        axis.plot(*points[[a, b]].T, color="black", linewidth=1)
    for number, node in enumerate(node_ids):
        axis.scatter(*points[number], color="black", s=12)
        axis.annotate(
            str(number), points[number], xytext=(3, 3), textcoords="offset points"
        )
    for cap in report["source_terminal_cuts"]:
        axis.plot(
            *inputs.surface.vertices[list(cap["original_vertices"]), :2].T,
            color="crimson",
            linewidth=2,
        )
    axis.autoscale()
    axis.set_aspect("equal")
    axis.set(
        xlabel="Source east [m]",
        ylabel="Source north [m]",
        title="Actual source axes / whole conforming parts\nRed: generated end sections, not qualified contacts",
    )
    view = figure.add_subplot(122, projection="3d")
    origin = float(mesh.vertices[:, 2].mean())
    view.plot_trisurf(
        *np.c_[mesh.vertices[:, :2], mesh.vertices[:, 2] - origin].T,
        triangles=mesh.triangles,
        color="#277da8",
        edgecolor=".3",
        linewidth=0.2,
    )
    old = inputs.surface.vertices.copy()
    old[:, 2] -= origin
    view.scatter(*old.T, color="crimson", s=9, label="original DEM observations")
    view.set(
        xlabel="Source east [m]",
        ylabel="Source north [m]",
        zlabel=f"Height minus {origin:.3f} m",
        title="Whole surface and original DEM priors\nSource admission remains Pending",
    )
    view.set_box_aspect(np.maximum(np.ptp(mesh.vertices, axis=0), 0.1))
    view.legend(fontsize=8)
    figure.suptitle(title)
    figure.savefig(output / "geometry.png", dpi=130)
    plt.close(figure)
