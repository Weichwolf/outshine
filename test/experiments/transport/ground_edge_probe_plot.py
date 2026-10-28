"""Actual same-XY residuals and stored meshes; diagnostic colours, no PBR claim."""

import json

import numpy as np


def draw(output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    report = json.loads((output / "report.json").read_text())
    arrays = np.load(output / "geometry.npz", allow_pickle=False)
    methods = tuple(report["methods"])
    cases = tuple(report["same_fixed_xy_validation"])
    fig, axes = plt.subplots(
        len(cases), len(methods), figsize=(17, 11), layout="constrained"
    )
    for row, key in enumerate(cases):
        xy = arrays[key + "_validation_xy"]
        common = arrays[key + "_common_soil"]
        limit = max(
            float(abs(arrays[key + "_" + method + "_error"][common]).max())
            for method in methods
        )
        for col, method in enumerate(methods):
            error = arrays[key + "_" + method + "_error"]
            ax = axes[row, col]
            image = ax.scatter(
                *xy[common].T,
                c=error[common],
                s=2,
                cmap="coolwarm",
                vmin=-limit,
                vmax=limit,
                rasterized=True,
            )
            metrics = report["same_fixed_xy_validation"][key]["methods"][method][
                "same_common_soil"
            ]
            detail = report["methods"][method]["cases"][key]["refinement"][
                "new_vertices"
            ]
            ax.set(
                aspect="equal",
                xlabel="east m",
                ylabel="north m",
                title=f"{key} / {method}: {detail} new vertices\nmax {metrics['maximum_abs_m']:.4f} m, RMS {metrics['rms_m']:.4f} m",
            )
            fig.colorbar(image, ax=ax, label="height minus bound DEM m", shrink=0.65)
    fig.suptitle(
        "Same fixed XY controls, unchanged roads and outer profiles, identical capacities\nSampled DEM residuals, no continuous error bound or native scene/device admission"
    )
    fig.savefig(output / "same-controls.png", dpi=135)
    plt.close(fig)
    fig = plt.figure(figsize=(14, 6), layout="constrained")
    for col, method in enumerate(("midpoint", "quartiles")):
        record = report["methods"][method]["cases"]["loop25"]
        prefix = record["array_prefix"]
        vertices, faces = (
            arrays[prefix + "mesh_vertices"],
            arrays[prefix + "mesh_faces"],
        )
        q = vertices[faces]
        normal = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])
        normal /= np.linalg.norm(normal, axis=1)[:, None]
        light = np.array([-1.0, 1.0, 1.0]) / np.sqrt(3)
        diffuse = 0.2 + 0.8 * np.maximum(normal @ light, 0)
        colours = np.tile([0.64, 0.72, 0.52], (len(faces), 1)) * diffuse[:, None]
        colours[:142] = np.array([0.45, 0.45, 0.45]) * diffuse[:142, None]
        base = float(vertices[:, 2].min())
        q = q.copy()
        q[:, :, 2] -= base
        low, high = vertices.min(axis=0), vertices.max(axis=0)
        ax = fig.add_subplot(1, 2, col + 1, projection="3d")
        ax.add_collection3d(
            Poly3DCollection(q, facecolors=colours, edgecolors=".35", linewidth=0.2)
        )
        ax.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(0, high[2] - low[2]),
            xlabel="east m",
            ylabel="north m",
            zlabel=f"height minus {base:.3f} m",
            title=f"{method}: actual {len(vertices)} vertices / {len(faces)} faces\n{record['quality']['whole_road_soil_edges']} unchanged whole road contacts",
        )
        ax.set_box_aspect(high - low)
        ax.view_init(elev=30, azim=-60)
    fig.suptitle(
        "Whole original25-span/six-loop component: complete stored terrain geometry\nPhysical aspect, diagnostic diffuse shading; no native PBR/Source/World/device admission"
    )
    fig.savefig(output / "geometry.png", dpi=135)
    plt.close(fig)
