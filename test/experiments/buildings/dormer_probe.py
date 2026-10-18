"""Actual dormer roof plans, sections and matched1920×1080 building/crop products."""

import argparse
from dataclasses import replace
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_mesh import slice_triangles
from building_plan_views import patch as draw_polygon
from clad_facade_probe import export
from dormer_sources import sources
from dormer_windows import DEPTH_M, aperture
from evidence import revision
from stepped_source_probe import overview
from tile_dormers import DormerFacades
from tile_elevations import ElevationFacades
from tile_facade_recess import demand
from verify_elevations import build
from verify_facade_frames import view_for


def diagrams(output, index, row, asset):
    fig, axes = plt.subplots(1, 2, figsize=(19.2, 10.8))
    draw_polygon(axes[0], row["polygon"], facecolor="#d5d2c9", edgecolor="#777")
    for p in asset.dormers:
        draw_polygon(
            axes[0],
            shapely.from_wkb(p.footprint_wkb),
            facecolor="#627d8a",
            edgecolor="#344653",
        )
        axes[0].plot(*np.asarray([p.a, p.b]).T, color="#d09e60", lw=2)
    axes[0].set(
        aspect="equal",
        title="Tatsächliche Dächer/Höfe und disjunkte Gauben",
        xlabel="Ost [m]",
        ylabel="Nord [m]",
    )
    axes[0].autoscale_view()
    p = asset.dormers[0]
    centre, w, h, z = aperture(p)
    tangent = (np.asarray(p.b) - p.a) / p.width_m
    inward = np.array([-tangent[1], tangent[0]])
    for product, label, colour in (
        (asset.original, "Originaldach", "#a7aaa5"),
        (asset.full.physical, "Vereinigter Körper", "#c39158"),
        (asset.near_products().physical, "Echte Fenstervertiefung", "#375f78"),
    ):
        lines = slice_triangles(product.mesh, np.r_[tangent, 0.0], centre @ tangent)
        for points, _material in lines:
            axes[1].plot(
                (points[:, :2] - centre) @ inward, points[:, 2], color=colour, lw=1.4
            )
        axes[1].plot([], [], color=colour, label=label)
    axes[1].plot([0.18, 0.18], [z, z + h], color="#7fadb2", ls="--", label="Glaslage")
    axes[1].set(
        aspect="equal",
        xlim=(-0.3, 2.4),
        ylim=(asset.eave_m - 0.3, p.cap_m + 0.5),
        xlabel="Abstand hinter Gaubenfront [m]",
        ylabel="Z [m]",
        title="Schnitt durch tatsächliche geschlossene Körper",
    )
    axes[1].legend(fontsize=8)
    fig.suptitle(row["id"] + f" · {len(asset.dormers)} tatsächliche Dachgauben")
    fig.tight_layout()
    fig.savefig(output / f"{index}-dormer-plan.png", dpi=100)
    plt.close(fig)


def detail_view(asset):
    p = asset.dormers[0]
    xy = shapely.get_coordinates(shapely.from_wkb(p.footprint_wkb))
    points = np.asarray([[x, y, z] for x, y in xy for z in (p.base_m, p.cap_m)])
    t = (np.asarray(p.b) - p.a) / p.width_m
    return overview(points, np.r_[[t[1], -t[0]] + 0.15 * t, 0.28])


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze actual dormer producer before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index, (row, neighbors) in enumerate(sources()):
        primary = view_for(row)
        _mesh, _info, owner = build(DormerFacades, row, neighbors, primary)
        asset = owner.base.dormer_assets[row["id"]]
        diagrams(output, index, row, asset)
        if index == 1:
            continue
        a, b, c, d = row["polygon"].bounds
        top = float(asset.original.mesh["vertices"][:, 2].max())
        points = np.array([(x, y, z) for x in (a, c) for y in (b, d) for z in (0, top)])
        far = replace(
            primary, eye=primary.target + (primary.eye - primary.target) * 2.4
        )
        while any(
            demand(p.frame(), p.base_m, far, depth=DEPTH_M) is not None
            for p in asset.dormers
        ):
            far = replace(far, eye=far.target + (far.eye - far.target) * 1.4)
        views = (
            ("Near", primary),
            ("Opposite", overview(points, [-1, 1.25, 0.8])),
            ("Elevated", overview(points, [1, -1.25, 1.8])),
            ("Far", far),
            ("Detail", detail_view(asset)),
        )
        for label, view in views:
            for suffix, kind in (
                ("Before", ElevationFacades),
                ("After", DormerFacades),
            ):
                mesh, info, owner = build(kind, row, neighbors, view)
                export(
                    output,
                    f"{index}{label}{suffix}",
                    mesh,
                    info | {"probe_facade_diagnostics": owner.report()},
                    view,
                    owner,
                    proof,
                )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a subdirectory of /tmp")
    models(output)


if __name__ == "__main__":
    main()
