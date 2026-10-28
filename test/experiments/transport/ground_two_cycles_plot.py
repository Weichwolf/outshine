"""Actual complete two-loop geometry, original axes and DEM observations."""

import numpy as np


def draw(output, results):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    accepted = [(key, row["result"]) for key, row in results.items() if "result" in row]
    figure = plt.figure(figsize=(13, 5 * len(accepted)), layout="constrained")
    for row, (key, result) in enumerate(accepted):
        mesh, inputs, geometry = (result[k] for k in ("mesh", "inputs", "geometry"))
        axis = figure.add_subplot(len(accepted), 2, row * 2 + 1)
        axis.add_collection(
            PolyCollection(
                mesh.vertices[mesh.triangles, :2],
                array=result["face_parts"],
                cmap="tab20",
                edgecolors=".4",
                linewidths=0.4,
            )
        )
        xy = np.asarray(geometry["source_nodes_EN"])
        for a, b in geometry["source_edges_local"]:
            axis.plot(*xy[[a, b]].T, color="black", linewidth=1)
        axis.scatter(*xy.T, s=12, color="black")
        axis.autoscale()
        axis.set_aspect("equal")
        axis.set(
            xlabel="source east m",
            ylabel="source north m",
            title=f"Original component {key}\n12 complete spans, two preserved source holes",
        )
        view = figure.add_subplot(len(accepted), 2, row * 2 + 2, projection="3d")
        origin = float(mesh.vertices[:, 2].mean())
        xyz = mesh.vertices.copy()
        xyz[:, 2] -= origin
        view.plot_trisurf(
            *xyz.T,
            triangles=mesh.triangles,
            color="#277da8",
            edgecolor=".3",
            linewidth=0.3,
        )
        prior = inputs.surface.vertices.copy()
        prior[:, 2] -= origin
        view.scatter(
            *prior.T, color="crimson", s=8, label="original bound DEM observations"
        )
        view.set_box_aspect(np.maximum(np.ptp(mesh.vertices, axis=0), 0.1))
        view.set(
            xlabel="east m",
            ylabel="north m",
            zlabel=f"height minus {origin:.3f} m",
            title=f"Actual complete {len(mesh.vertices)} vertices / {len(mesh.triangles)} triangles\nPhysical aspect, Source admission remains Pending",
        )
        view.legend(fontsize=7)
    figure.suptitle(
        "Two original bounded ground components: complete shared sections and full C1/finite-grade checks\nNo original-qualified ends, canonical Source/World/terrain/device admission"
    )
    figure.savefig(output / "geometry.png", dpi=135)
    plt.close(figure)
