"""Inspect actual Float32 triangle coordinates; no device-render claim."""

import numpy as np
import shapely


def display(plan, data, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import MaxNLocator
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    vertices = data["body_vertices"].astype(np.float32).astype(float)
    triangles = vertices[np.vstack([data["top_faces"], data["wall_faces"]])]
    stair = plan["stairs"][0][0]
    top = data["top_faces"][(data["face_sources"] & (1 << stair)) != 0]
    walls = data["wall_faces"]
    inside = shapely.covers(
        plan["prepared_domains"][stair].buffer(1e-10),
        shapely.points(vertices[walls, :2]),
    ).all(axis=1)
    detail = vertices[np.vstack([top, walls[inside]])]
    figure = plt.figure(figsize=(13, 7))
    for index, chosen in enumerate((triangles, detail)):
        axes = figure.add_subplot(1, 2, index + 1, projection="3d")
        normal = np.cross(chosen[:, 1] - chosen[:, 0], chosen[:, 2] - chosen[:, 0])
        shade = 0.45 + 0.55 * abs(normal[:, 2]) / np.linalg.norm(normal, axis=1)
        color = shade[:, None] * np.array([0.57, 0.72, 0.53])
        axes.add_collection3d(
            Poly3DCollection(
                chosen, facecolors=color, edgecolors=(0, 0, 0, 0.15), linewidths=0.25
            )
        )
        if index == 0:
            for profile in plan["native_cut_context"]["profiles"]:
                xyz = (
                    np.array(profile["profile_xyz_m"]).astype(np.float32).astype(float)
                )
                axes.plot(
                    xyz[:, 0], xyz[:, 1], xyz[:, 2], "k.-", linewidth=1, markersize=3
                )
        low, high = chosen.min(axis=(0, 1)), chosen.max(axis=(0, 1))
        axes.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2], high[2]),
            xlabel="east / m",
            ylabel="north / m",
            zlabel="height / m",
            title="Four complete native frontiers"
            if index == 0
            else "Actual stair-owned treads, landings and risers",
        )
        axes.set_box_aspect(high - low)
        axes.zaxis.set_major_locator(MaxNLocator(5))
        axes.view_init(elev=32, azim=-60)
    figure.suptitle(
        "Local Python body at Float32 coordinates; native outside integration remains open"
    )
    figure.tight_layout()
    figure.savefig(output / "native-body.png", dpi=170)
    plt.close(figure)
