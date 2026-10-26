"""Render the actual generated triangles, not inferred accepted solver status."""

import numpy as np


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
