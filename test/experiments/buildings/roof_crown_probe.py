"""Real roof partitions, current facade products and matched 1920×1080 cameras."""

import argparse
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_plan_views import patch as draw_polygon
from clad_facade_mesh import build as previous
from clad_facade_probe import export
from evidence import revision
from roof_crown_sources import sources
from stepped_source_probe import overview
from tile_roof_crowns import CrownFacades, error
from provider_contract import attributes


def diagram(output, index, row, crown):
    fig, axes = plt.subplots(1, 2, figsize=(19.2, 10.8))
    colours = ("#475d6b", "#8fa9b8", "#ddd6be", "#728f99", "#85989a")
    for region, colour in zip(crown.regions, colours, strict=False):
        draw_polygon(
            axes[0],
            shapely.from_wkb(region.footprint_wkb),
            facecolor=colour,
            edgecolor="none",
        )
        axes[1].bar(
            region.name, region.high_m - region.low_m, bottom=region.low_m, color=colour
        )
        axes[1].scatter(region.name, region.high_m, color="black", s=18)
    axes[0].set(
        aspect="equal",
        title="Tatsächliche 2D-Teilflächen · Höfe bleiben frei",
        xlabel="Ost [m]",
        ylabel="Nord [m]",
    )
    axes[0].autoscale_view()
    axes[1].set(
        title="Absolute Höhen der Teilflächen · Linien = flache Flächen",
        ylabel="Höhe [m]",
        ylim=(0, crown.top_m * 1.1),
    )
    axes[1].tick_params(axis="x", rotation=20)
    axes[1].axhline(
        crown.deck_m,
        linestyle="--",
        color="#917a54",
        label="Deckdatum vor Fassadenplanung",
    )
    axes[1].legend()
    fig.suptitle(row["id"] + " · " + crown.kind)
    fig.tight_layout()
    fig.savefig(output / f"{index}-crown-plan.png", dpi=100)
    plt.close(fig)


def build(row, neighbors, view, forced=None):
    owner = CrownFacades([row, *neighbors], view)
    source = owner.rows[0]
    if forced is None:
        mesh, info = owner(source, attributes(source["properties"]), 12)
    else:
        with patch(
            "tile_roof_crowns.error",
            return_value=float("inf") if forced == "full" else 0.0,
        ):
            mesh, info = owner(source, attributes(source["properties"]), 12)
    if "roof_crown_plan" not in info or owner.rejected:
        raise ValueError("Actual source lost crown/facade programme")
    return mesh, info | {"probe_facade_diagnostics": owner.report()}, owner


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze roof crown producers before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index, (row, neighbors) in enumerate(sources()):
        a, b, c, d = row["polygon"].bounds
        top = attributes(row["properties"])["buildingHeight"]
        points = np.array([(x, y, z) for x in (a, c) for y in (b, d) for z in (0, top)])
        primary = overview(points, [1.0, -1.25, 0.65])
        _mesh, _info, owner = build(row, neighbors, primary)
        crown = owner.base.assets[row["id"]].plan
        diagram(output, index, row, crown)
        if index not in (0, 3):
            continue
        opposite = overview(points, [-1.0, 1.25, 0.65])
        elevated = overview(points, [1.0, -1.25, 1.8])
        far = replace(primary, eye=primary.target + (primary.eye - primary.target) * 2)
        while error(crown, far) > 0.5:
            far = replace(far, eye=far.target + (far.eye - far.target) * 1.5)
        for label, view in (
            ("Near", primary),
            ("Opposite", opposite),
            ("Elevated", elevated),
            ("Far", far),
        ):
            before, info, previous_owner = previous(row, neighbors, view)
            after, changed, current_owner = build(row, neighbors, view)
            export(
                output,
                f"{index}{label}Before",
                before,
                info,
                view,
                previous_owner,
                proof,
            )
            export(
                output,
                f"{index}{label}After",
                after,
                changed,
                view,
                current_owner,
                proof,
            )
        for label, view, forced in (
            ("FarFull", far, "full"),
            ("NearCoarse", primary, "coarse"),
        ):
            mesh, info, owner = build(row, neighbors, view, forced)
            export(
                output,
                f"{index}{label}",
                mesh,
                info | {"forced_countercontrol": forced},
                view,
                owner,
                proof,
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target = args.output.resolve()
    if target == Path("/tmp").resolve() or not target.is_relative_to(
        Path("/tmp").resolve()
    ):
        parser.error("--output requires a subdirectory of /tmp")
    models(target)


if __name__ == "__main__":
    main()
