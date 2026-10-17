"""Actual source elevations, 2D opening plans and matched 1920×1080 products."""

import argparse
from dataclasses import replace
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from clad_facade_probe import export
from elevation_sources import sources
from evidence import revision
from stepped_source_probe import overview
from tile_elevations import ElevationFacades
from tile_roof_crowns import CrownFacades
from verify_elevations import build


def diagram(output, index, row, plans):
    p = max(plans, key=lambda p: p.width_m)
    fig, ax = plt.subplots(figsize=(19.2, 10.8))
    for scope, colour in zip(p.scopes, ("#d8c5a8", "#e4e1d9", "#b1bdc2"), strict=True):
        height = scope.top_m - scope.base_m
        ax.add_patch(plt.Rectangle((0, scope.base_m), p.width_m, height, color=colour))
        cell, storey = p.width_m / scope.columns, height / scope.rows
        w, h = cell * scope.fractions[0], storey * scope.fractions[1]
        for c in range(scope.columns):
            for r in range(scope.rows):
                ax.add_patch(
                    plt.Rectangle(
                        (
                            (c + 0.5) * cell - w / 2,
                            scope.base_m + (r + 0.5) * storey - h / 2,
                        ),
                        w,
                        h,
                        facecolor="#304853",
                    )
                )
        ax.text(
            p.width_m + 0.6,
            scope.base_m + height / 2,
            f"{scope.name}: {scope.columns} × {scope.rows}\n{w:.2f} × {h:.2f} m",
        )
    ax.set(
        aspect="equal",
        xlim=(-0.5, p.width_m + 9),
        ylim=(-0.5, p.scopes[-1].top_m + 0.5),
        xlabel="Fassaden-U [m]",
        ylabel="Z [m]",
        title=row["id"] + " · " + p.kind + " · tatsächlicher Quellplan",
    )
    fig.tight_layout()
    fig.savefig(output / f"{index}-elevation-plan.png", dpi=100)
    plt.close(fig)


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze scoped facade producer before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index in (0, 3):
        row, neighbors = sources()[index]
        a, b, c, d = row["polygon"].bounds
        from provider_contract import attributes

        top = attributes(row["properties"])["buildingHeight"]
        points = np.array([(x, y, z) for x in (a, c) for y in (b, d) for z in (0, top)])
        primary = overview(points, [1.0, -1.25, 0.65])
        _mesh, _info, owner = build(ElevationFacades, row, neighbors, primary)
        diagram(output, index, row, owner.elevations[row["id"]])
        cameras = (
            ("Near", primary),
            ("Opposite", overview(points, [-1.0, 1.25, 0.65])),
            ("Elevated", overview(points, [1.0, -1.25, 1.8])),
            (
                "Far",
                replace(
                    primary, eye=primary.target + (primary.eye - primary.target) * 2.4
                ),
            ),
        )
        for label, view in cameras:
            for suffix, owner_type in (
                ("Before", CrownFacades),
                ("After", ElevationFacades),
            ):
                mesh, info, owner = build(owner_type, row, neighbors, view)
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
