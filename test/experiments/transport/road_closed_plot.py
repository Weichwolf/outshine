"""Actual isolated method geometry from below; reconstructed reference, no PBR."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import MaxNLocator
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from road_closed_surface import closed_indices
from road_surface_frames import differentials, unit
from road_surface_lod import prepare
from road_surface_model import Curve


def sampled_reference(source):
    curve = Curve(source)
    stations = np.unique(np.r_[curve.stations, np.linspace(*source["range_m"], 257)])
    top = curve.at(stations).reshape(-1, 3)
    along, across = differentials(curve, stations)
    normal = unit(np.cross(along, across)).reshape(-1, 3)
    bands = np.array([1]) if curve.shoulder == 0 else np.arange(3)
    a = np.arange(len(stations) - 1)[:, None] * 4 + bands
    faces = np.stack((a, a + 4, a + 1, a + 1, a + 4, a + 5), axis=-1).reshape(-1, 3)
    used = np.unique(faces)
    mapping = np.full(len(top), -1)
    mapping[used] = np.arange(len(used))
    positions = np.vstack((top[used], top[used] - curve.thickness * normal[used]))
    return positions, closed_indices(mapping[faces].astype(np.uint32), len(used))


def profile(panel, center, left, upper, lower, label, style):
    points = np.vstack((upper, lower[::-1], upper[:1]))
    panel.plot(
        (points[:, :2] - center[:2]) @ left,
        points[:, 2] - center[2],
        style,
        label=label,
    )


def draw(cases, results, output):
    original = {case["ordinal"]: case for case in cases}
    common = [
        key
        for key in results["adaptive"]
        if "body" in results["adaptive"][key] and "body" in results["uniform"][key]
    ]
    efficient = max(
        common,
        key=lambda key: (
            len(results["uniform"][key]["body"]["indices"])
            / len(results["adaptive"][key]["body"]["indices"])
        ),
    )
    tilted = max(
        (key for key in common if key != efficient),
        key=lambda key: results["adaptive"][key]["body"][
            "normal_offset_departure_bound_m"
        ],
    )
    figure = plt.figure(figsize=(13, 10), layout="constrained")
    for row, ordinal in enumerate((efficient, tilted)):
        source = original[ordinal]["recipe"]
        body = results["adaptive"][ordinal]["body"]
        vertices, faces = body["positions_enh_m"], body["indices"]
        low, high = vertices.min(axis=0), vertices.max(axis=0)
        axis = figure.add_subplot(2, 2, row * 2 + 1, projection="3d")
        triangles = vertices[faces].copy()
        triangles[:, :, 2] -= low[2]
        axis.add_collection3d(
            Poly3DCollection(triangles, facecolor=".7", edgecolor=".35", linewidth=0.2)
        )
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(0, high[2] - low[2]),
            xlabel="east m",
            ylabel="north m",
            zlabel=f"height minus{low[2]:.3f}m",
            title=f"Original{ordinal}: complete adaptive body\n{len(faces)} triangles; uniform {len(results['uniform'][ordinal]['body']['indices'])}",
        )
        axis.set_box_aspect(high - low)
        axis.view_init(elev=-24, azim=-65)
        axis.tick_params(labelsize=7)
        for dimension in (axis.xaxis, axis.yaxis, axis.zaxis):
            dimension.set_major_locator(MaxNLocator(4))
        panel = figure.add_subplot(2, 2, row * 2 + 2)
        curve = Curve(source)
        reference, _ = sampled_reference(source)
        n = len(reference) // 2
        # The dense reference ends in its four or two active section vertices.
        columns = 2 if curve.shoulder == 0 else 4
        ref_upper = reference[n - columns : n]
        ref_lower = reference[2 * n - columns :]
        center = curve.at(np.array([curve.stations[-1]]))[0].mean(axis=0)
        heading = curve.poses[-1, 4]
        left = np.array([-np.sin(heading), np.cos(heading)])

        profile(
            panel,
            center,
            left,
            ref_upper,
            ref_lower,
            "Analytic normal-offset rule",
            "k-",
        )
        for method, style in (("uniform", "g--"), ("adaptive", "r:")):
            result = results[method][ordinal]["body"]
            top = result["upper_vertex_count"]
            xy = result["positions_enh_m"]
            profile(
                panel,
                center,
                left,
                xy[top - columns : top],
                xy[2 * top - columns :],
                method + " closed cap",
                style,
            )
        old = prepare(curve, 0.25)["positions_enh_m"][-1]
        panel.plot(
            (old[:, :2] - center[:2]) @ left,
            old[:, 2] - center[2],
            "bo-",
            label="Existing Upper section",
        )
        panel.set(
            aspect="auto",
            xlabel="signed lateral projection m",
            ylabel="height relative to source end m",
            title="Actual terminal section, lateral/up projection",
        )
        panel.grid(alpha=0.25)
        panel.legend(fontsize=7, loc="best")
    figure.suptitle(
        "Actual pre-contact road recipes,0.3m original normal depth,0.25m complete-body budget\n3D at physical aspect; section charts use independent axis scales. No native full-body oracle/PBR/ports/World/device acceptance"
    )
    figure.savefig(output / "closed-road-comparison.png", dpi=150)
    plt.close(figure)
