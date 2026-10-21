"""Render the actual generated triangles, not inferred accepted solver status."""

import numpy as np


def display_compact_flights(plan, data, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import MaxNLocator
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    triangles = data["body_vertices"][
        np.vstack([data["top_faces"], data["wall_faces"]])
    ]
    centre = triangles.mean(axis=1)
    for flight in plan["compact_flights"]:
        owner = next(
            i
            for i, r in enumerate(plan["rows"])
            if r["source_id"] == flight["source_id"]
        )
        low_x, low_y, high_x, high_y = plan["prepared_domains"][owner].bounds
        pad = plan["rows"][owner]["width_m"] / 4
        chosen = triangles[
            (centre[:, 0] >= low_x - pad)
            & (centre[:, 0] <= high_x + pad)
            & (centre[:, 1] >= low_y - pad)
            & (centre[:, 1] <= high_y + pad)
        ]
        low_z, high_z = float(chosen[:, :, 2].min()), float(chosen[:, :, 2].max())
        fig = plt.figure(figsize=(7, 6))
        axes = fig.add_subplot(projection="3d")
        axes.add_collection3d(
            Poly3DCollection(
                chosen,
                facecolors=(0.55, 0.67, 0.52),
                edgecolors=(0, 0, 0, 0.15),
                linewidths=0.3,
            )
        )
        axes.set(
            xlim=(low_x - pad, high_x + pad),
            ylim=(low_y - pad, high_y + pad),
            zlim=(low_z - 0.02, high_z + 0.02),
            xlabel="east / m",
            ylabel="north / m",
            zlabel="height / m",
            title=f"{flight['source_id']}: actual compact flight; boundary unqualified",
        )
        axes.set_box_aspect(
            (high_x - low_x + 2 * pad, high_y - low_y + 2 * pad, high_z - low_z + 0.04)
        )
        axes.zaxis.set_major_locator(MaxNLocator(4))
        axes.view_init(elev=32, azim=-60)
        fig.tight_layout()
        fig.savefig(output / f"compact-flight-{owner}.png", dpi=150)
        plt.close(fig)


def display(plan, heights, data, products, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(13, 9))
    scene = fig.add_subplot(221, projection="3d")
    vertices, faces = data["body_vertices"], data["body_faces"]
    points = vertices[faces]
    centre = points.mean(axis=1)
    focus = (
        (centre[:, 0] > -26)
        & (centre[:, 0] < -10)
        & (centre[:, 1] > 127)
        & (centre[:, 1] < 143)
    )
    chosen = points[focus]
    normals = np.cross(chosen[:, 1] - chosen[:, 0], chosen[:, 2] - chosen[:, 0])
    shade = 0.35 + 0.65 * abs(normals[:, 2]) / np.maximum(
        np.linalg.norm(normals, axis=1), 1e-30
    )
    collection = Poly3DCollection(
        chosen,
        facecolors=np.c_[shade * 0.65, shade * 0.72, shade * 0.62],
        edgecolors=(0, 0, 0, 0.08),
        linewidths=0.2,
    )
    scene.add_collection3d(collection)
    scene.set(
        xlim=(-26, -10),
        ylim=(127, 143),
        zlim=(27, 33),
        title="Actual closed stair/road body (local crop)",
    )
    scene.set_box_aspect((16, 16, 8))
    scene.view_init(elev=30, azim=-60)
    scene.set_xlabel("east / m")
    scene.set_ylabel("north / m")
    scene.set_zlabel("height / m")
    coverage = fig.add_subplot(222)
    for row, domain in zip(plan["rows"], plan["prepared_domains"], strict=True):
        x, y = domain.exterior.xy
        coverage.fill(x, y, alpha=0.15)
        coverage.plot(*row["xy"].T, ".-", lw=1, label=row["source_id"])
    for landing in products["landings"]:
        coverage.plot(*landing["xy_m"], "kx")
    context = products.get("region_context")
    if context is not None:
        selected = set(context["selected_source_ids"])
        for way in context["capture"]["ways"]:
            key = f"native-part:{way['native_way']}"
            if key not in selected:
                xy = np.array(way["source_xy_m"])
                coverage.plot(*xy.T, "--", color="gray", lw=3, label=f"outside {key}")
        ports = products["external_contacts"]
        if ports:
            xy = np.array([p["profile_xyz_m"][0][:2] for p in ports])
            coverage.scatter(
                *xy.T,
                s=75,
                facecolors="none",
                edgecolors="red",
                label="unqualified boundary",
            )
    coverage.set_aspect("equal")
    coverage.set_title(f"All {len(plan['rows'])} retained corridors and shared nodes")
    coverage.legend(fontsize=8)
    coverage.grid(alpha=0.2)
    for index, owner in enumerate((0, 2)):
        axes = fig.add_subplot(2, 2, 3 + index)
        row = plan["rows"][owner]
        prior = row["height_prior_xyz_m"]
        prior_arc = np.r_[
            0, np.cumsum(np.linalg.norm(np.diff(prior[:, :2], axis=0), axis=1))
        ]
        xyz = np.array(products["navigation"]["axis_profiles_xyz_m"][row["source_id"]])
        arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(xyz[:, :2], axis=0), axis=1))]
        axes.plot(prior_arc, prior[:, 2], "o--", label="soft source height samples")
        axes.plot(arc, xyz[:, 2], ".-", label="shared solved surface envelope")
        if row["original"]["source"]["feature"]["properties"]["subclass"] == "steps":
            for level in data["levels_m"]:
                axes.axhline(level, color="gray", lw=0.5, alpha=0.4)
        axes.set(title=row["source_id"], xlabel="axis station / m", ylabel="height / m")
        axes.grid(alpha=0.2)
        axes.legend(fontsize=8)
    fig.suptitle(
        "Local shared 2.5D proposal; external contacts and native admission pending"
    )
    fig.tight_layout()
    fig.savefig(output, dpi=150)
    plt.close(fig)
