"""Same actual source/context and camera before/after joined frame hierarchies."""

import argparse
import json
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_mesh import slice_triangles
from demand import View
from evidence import revision
from facade_entry_geometry import basis_for
from matplotlib.collections import LineCollection
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from outshine_ref.buildings.facade_frames import FacadeFrame, frame_surface, region
from recess_probe import building
from stepped_source_probe import overview
from verify_facade_frames import build, sources, view_for


def diagram(record, row, output, index):
    frame = FacadeFrame(
        **{
            k: tuple(v) if isinstance(v, list) else v
            for k, v in record["frame"].items()
        }
    )
    mesh = frame_surface(frame)
    polygon = region(frame)
    figure = plt.figure(figsize=(19.2, 12))
    axis = figure.add_subplot(221)
    axis.fill(*np.asarray(polygon.exterior.coords).T, color="#c4b49a")
    for ring in polygon.interiors:
        axis.fill(*np.asarray(ring.coords).T, color="white")
    axis.set(
        aspect="equal",
        xlabel="Facade U [m]",
        ylabel="V above frame base [m]",
        title="2D · actual frame region / free apertures",
    )
    for panel, label, normal, offset, dims, colour in (
        (
            222,
            "Pier horizontal",
            [0, 1, 0],
            (frame.horizontal[0] + frame.horizontal[1]) / 2,
            (0, 2),
            "#537b91",
        ),
        (
            223,
            "Beam vertical",
            [1, 0, 0],
            (frame.vertical[0] + frame.vertical[1]) / 2,
            (2, 1),
            "#ae6d3e",
        ),
    ):
        axis = figure.add_subplot(panel)
        lines = [p[:, dims] for p, _ in slice_triangles(mesh, normal, offset)]
        axis.add_collection(
            LineCollection(lines, colors=colour, linewidths=1.4, label=label)
        )
        if panel == 222:
            axis.set(
                xlim=(frame.vertical[1] - 0.8, frame.vertical[1] + 0.8),
                ylim=(-0.05, frame.depth_m + 0.05),
                xlabel="U [m]",
                ylabel="Outward [m]",
            )
        else:
            centre = (
                frame.horizontal[1]
                if len(frame.horizontal) > 2
                else frame.horizontal[0]
            )
            axis.set(
                xlim=(-0.05, frame.depth_m + 0.05),
                ylim=(centre - 0.8, centre + 0.8),
                xlabel="Outward [m]",
                ylabel="V [m]",
            )
        axis.set(aspect="equal", title="Exact physical mesh section · " + label)
        axis.grid(alpha=0.15)
    spatial = figure.add_subplot(224, projection="3d")
    triangles = mesh["vertices"][mesh["faces"]][:, :, [0, 2, 1]]
    normal = mesh["normals"][mesh["faces"][:, 0]][:, [0, 2, 1]]
    light = np.array([-0.4, -0.7, 0.59160798])
    colours = (
        np.array([0.77, 0.72, 0.63])
        * (0.55 + 0.45 * np.maximum(normal @ light, 0))[:, None]
    )
    spatial.add_collection3d(
        Poly3DCollection(triangles, facecolors=colours, edgecolors="none")
    )
    spatial.set(
        xlim=(0, frame.width_m),
        ylim=(-0.03, frame.depth_m + 0.03),
        zlim=(0, frame.height_m),
        title="2.5D · exact closed reference frame, wall hidden",
    )
    spatial.set_box_aspect((frame.width_m, frame.depth_m + 0.06, frame.height_m))
    spatial.set_yticks([])
    spatial.set_ylabel(f"Outward 0..{frame.depth_m:.2f} m")
    spatial.view_init(elev=20, azim=-65)
    figure.suptitle(
        row["id"] + " · " + record["kind"] + " · unchanged window/storey axes"
    )
    figure.tight_layout()
    figure.savefig(output / f"{index}-frame-plan-section-spatial.png", dpi=100)
    plt.close(figure)


def views(points, info, records):
    selected = max(
        records, key=lambda r: r["frame"]["width_m"] * r["frame"]["height_m"]
    )
    tangent = np.asarray(selected["tangent"])
    normal = np.array([tangent[1], -tangent[0]])
    facade = next(f for f in info["facade_plan"] if f["id"] == selected["facade_id"])
    basis = basis_for(facade)
    parameters = selected["frame"]
    origin = np.r_[selected["origin_xy_m"], selected["base_m"]]
    centre = (
        origin
        + basis[0] * parameters["width_m"] * 0.5
        + [0, 0, min(parameters["height_m"] / 2, 5)]
    )
    whole = overview(points, [*(normal + tangent * 0.35), 0.8])
    return selected, (
        ("Whole", whole),
        (
            "Opposite",
            overview(points, [*(-normal + tangent * 0.35), 0.8]),
        ),
        (
            "Near",
            View(
                centre + np.r_[normal * 7 + tangent * 0.5, 0.3],
                centre,
                width=1920,
                height=1080,
                fov_deg=48,
            ),
        ),
        ("Far", replace(whole, eye=whole.target + (whole.eye - whole.target) * 1000)),
    )


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before facade programme comparisons")
    output.mkdir(parents=True, exist_ok=True)
    cases = []
    for index, (row, neighbors) in enumerate(sources()):
        mesh, info, builder = build(row, neighbors, view_for(row))
        if not builder.frames.completed:
            raise ValueError("Actual source produced no admitted hierarchy")
        selected, cameras = views(mesh["vertices"], info, builder.frames.completed)
        diagram(selected, row, output, index)
        for label, view in cameras:
            with patch("facade_frame_geometry.choose", return_value=((), [])):
                before = building(
                    row,
                    view,
                    f"{index}Before{label}",
                    output,
                    proof,
                    context_rows=neighbors,
                )
            after = building(
                row,
                view,
                f"{index}Frames{label}",
                output,
                proof,
                context_rows=neighbors,
            )
            cases.append(
                {
                    "source_id": row["id"],
                    "camera": label,
                    "before": before,
                    "after": after,
                }
            )
    (output / "comparison.json").write_text(
        json.dumps(proof | {"cases": cases}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
