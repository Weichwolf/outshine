"""Local before/after diagnostic of the unchanged Ground and two end surfaces."""

import numpy as np


def plot_end_surfaces(path, ground, pieces):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(17, 5.5), layout="constrained")
    original = fig.add_subplot(131)
    before = fig.add_subplot(132)
    after = fig.add_subplot(133, projection="3d")
    original.triplot(
        ground.vertices[:, 0],
        ground.vertices[:, 1],
        ground.triangles,
        color=".55",
        linewidth=0.4,
    )
    original.set_aspect("equal")
    original.set(
        title="Before: qualified Ground only",
        xlabel="Source east [m]",
        ylabel="Source north [m]",
    )
    before.triplot(
        ground.vertices[:, 0],
        ground.vertices[:, 1],
        ground.triangles,
        color=".55",
        linewidth=0.4,
    )
    before.set_aspect("equal")
    before.set(
        title="Ground and actual partial Bridge end footprints",
        xlabel="Source east [m]",
        ylabel="Source north [m]",
    )
    after.add_collection3d(
        Poly3DCollection(
            ground.vertices[ground.triangles],
            facecolors=".75",
            edgecolors=".6",
            linewidths=0.2,
            alpha=0.65,
        )
    )
    arrays = [ground.vertices]
    for (name, mesh, row), color in zip(pieces, ("tab:blue", "tab:orange")):
        before.triplot(
            mesh.vertices[:, 0],
            mesh.vertices[:, 1],
            mesh.triangles,
            color=color,
            linewidth=0.8,
            label="Original group " + name,
        )
        before.plot(*mesh.vertices[:2, :2].T, color="black", linewidth=2)
        after.add_collection3d(
            Poly3DCollection(
                mesh.vertices[mesh.triangles],
                facecolors=color,
                edgecolors=color,
                linewidths=0.3,
            )
        )
        arrays.append(mesh.vertices)
    vertices = np.vstack(arrays)
    for ax in (original, before):
        ax.set_xlim(vertices[:, 0].min() - 0.5, vertices[:, 0].max() + 0.5)
        ax.set_ylim(vertices[:, 1].min() - 0.5, vertices[:, 1].max() + 0.5)
    for getter, axis in ((after.set_xlim, 0), (after.set_ylim, 1), (after.set_zlim, 2)):
        getter(vertices[:, axis].min() - 0.5, vertices[:, axis].max() + 0.5)
    after.set(
        xlabel="Source east [m]",
        ylabel="Source north [m]",
        zlabel="Model height [m]",
        title="Same affine terminal planes across whole shared caps",
    )
    after.view_init(elev=25, azim=-65)
    before.legend(fontsize=8)
    fig.suptitle(
        "Metric end surfaces only; opposite Ground, bodies, clearance and whole Bridge remain Pending"
    )
    fig.savefig(path, dpi=120, bbox_inches="tight")
    plt.close(fig)
