"""Actual source roof plans, mesh sections and spatial geometry before Place rendering."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_mesh import slice_triangles
from evidence import revision
from matplotlib.collections import LineCollection, PolyCollection
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from provider_contract import attributes
from tile_roof_envelopes import choose
from verify_residential_roofs import sources
from verify_roof_envelopes import benchmark, case


def figure(row, choice, envelope, top, output):
    polygon = row["polygon"]
    centre = np.asarray(polygon.centroid.coords[0])
    triangles = envelope.mesh["vertices"][envelope.mesh["faces"]].copy()
    triangles[:, :, :2] -= centre
    roof = top["vertices"][top["faces"]].copy()
    roof[:, :, :2] -= centre
    fig = plt.figure(figsize=(19.2, 10.8), dpi=100)
    ax = fig.add_subplot(2, 2, 1)
    ax.add_collection(
        PolyCollection(roof[:, :, :2], facecolors="#d3ac73", edgecolors="#594a38")
    )
    for ring in (polygon.exterior, *polygon.interiors):
        coordinates = np.asarray(ring.coords) - centre
        ax.plot(*coordinates.T, color="#1d3948", linewidth=1.5)
    ax.autoscale_view()
    ax.set_aspect("equal")
    ax.set_title("2D: original footprint / actual roof facets")
    ax.set_xlabel("east [m] relative to centroid")
    ax.set_ylabel("north [m] relative to centroid")
    ax = fig.add_subplot(2, 2, 3)
    roof_normals = np.cross(roof[:, 1] - roof[:, 0], roof[:, 2] - roof[:, 0])
    gradients = -roof_normals[:, :2] / roof_normals[:, 2:3]
    tangent = gradients[np.argmax(np.linalg.norm(gradients, axis=1))]
    tangent /= np.linalg.norm(tangent)
    normal = np.array([-tangent[1], tangent[0]])
    sections = slice_triangles(envelope.mesh, [*normal, 0.0], centre @ normal)
    lines = [
        np.column_stack([(points[:, :2] - centre) @ tangent, points[:, 2]])
        for points, _ in sections
    ]
    ax.add_collection(LineCollection(lines, colors="#38556b", linewidths=2))
    ax.axhline(
        choice.eave_m + choice.rise_m,
        color="#777777",
        linestyle="--",
        label="previous flat source top",
    )
    ax.axhline(
        choice.eave_m,
        color="#a66a29",
        linestyle=":",
        label="roof reserved before rooms",
    )
    ax.autoscale_view()
    ax.set_aspect("equal")
    ax.set_title("2D: actual mesh section / unchanged source height")
    ax.set_xlabel("along steepest roof slope [m]")
    ax.set_ylabel("height above source base [m]")
    ax.legend(fontsize=8)
    ax = fig.add_subplot(1, 2, 2, projection="3d")
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    colors = np.where(
        (normals[:, 2] > 0.2)[:, None], [0.56, 0.34, 0.18], [0.83, 0.82, 0.76]
    )
    light = np.array([-0.4, -0.6, 1.0])
    light /= np.linalg.norm(light)
    colors *= (0.60 + 0.40 * np.maximum(normals @ light, 0))[:, None]
    ax.add_collection3d(
        Poly3DCollection(
            triangles, facecolors=colors, edgecolors="#5c6063", linewidths=0.25
        )
    )
    coordinates = triangles.reshape(-1, 3)
    lower, upper = coordinates.min(axis=0), coordinates.max(axis=0)
    ax.set_xlim(lower[0], upper[0])
    ax.set_ylim(lower[1], upper[1])
    ax.set_zlim(lower[2], upper[2])
    ax.set_box_aspect(upper - lower)
    ax.view_init(elev=28, azim=-55)
    ax.set_title("2.5D: actual closed geometry / diagnostic colors")
    fig.suptitle(
        row["id"]
        + f"\n{choice.kind}: {polygon.area:.1f} m², roof rise {choice.rise_m:.2f} m",
        fontsize=14,
    )
    fig.text(
        0.04,
        0.025,
        "Procedural roof assumption; provider footprint and height retained. No fitted roof space / complete architecture family.",
        fontsize=10,
    )
    fig.subplots_adjust(left=0.06, right=0.97, top=0.86, bottom=0.10, hspace=0.4)
    fig.savefig(output)
    plt.close(fig)


def run(output, rooms=False):
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for index, row in enumerate(sources()):
        parameters = attributes(row["properties"])
        choice = choose(row, parameters)
        result, envelope, top, _ = case(row, parameters, choice, compare=True)
        if result["comparison_status"] != "pass":
            raise ValueError("Independent source roof comparison failed")
        image = f"house-{index}.png"
        figure(row, choice, envelope, top, output / image)
        if rooms:
            from pitched_room_details import complete

            try:
                mesh, info, _, _ = complete(row, parameters, 12, choice, top)
                np.savez_compressed(output / f"house-{index}-rooms.npz", **mesh)
                result["rooms"] = info
            except ValueError as error:
                result["rooms_rejected"] = str(error)
        results.append(
            result
            | {
                "choice": choice.__dict__,
                "image": image,
                "benchmark": benchmark(row, parameters, choice, 9),
            }
        )
    (output / "verification.json").write_text(
        json.dumps(revision() | {"cases": results}, indent=2) + "\n"
    )
    if rooms and any("rooms_rejected" in result for result in results):
        raise ValueError(
            "Requested source roof/room control failed; inspect verification.json"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rooms", action="store_true")
    args = parser.parse_args()
    run(args.output, args.rooms)
