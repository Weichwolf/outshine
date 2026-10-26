"""Actual support-zone plan crops: post-fit doors versus joint partition and placement."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_plan_views import patch
from building_vertical import level
from door_furnishing import joint
from door_support_plan import select
from office_support_plan import refine
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan
from shapely.affinity import translate


def draw(axis, plan, product, selection, bounds, origin, title):
    floor = level(plan, 0)
    patch(
        axis,
        translate(floor["footprint"], *-origin),
        facecolor="#515c66",
        edgecolor="none",
    )
    for room in floor["rooms"]:
        patch(axis, translate(room, *-origin), facecolor="#f0eee6", edgecolor="none")
    for record in product["rooms"]:
        for item in record["result"]["items"]:
            patch(
                axis,
                translate(item["activity"], *-origin),
                facecolor="#c6dcba",
                edgecolor="none",
                alpha=0.7,
            )
            patch(
                axis,
                translate(item["footprint"], *-origin),
                facecolor="#aa8459",
                edgecolor="#705034",
                linewidth=0.6,
            )
        for path in record["result"]["proof"]["paths"]:
            xy = np.asarray(path.coords) - origin
            axis.plot(xy[:, 0], xy[:, 1], color="#398778", linewidth=0.8)
    for door in floor["doors"]:
        patch(
            axis,
            translate(door["polygon"], *-origin),
            facecolor="#f0eee6",
            edgecolor="none",
        )
    for row in selection["selected"]:
        patch(
            axis,
            translate(row["motion"], *-origin),
            facecolor="#a7beda",
            edgecolor="#557ba3",
            alpha=0.5,
        )
        patch(
            axis,
            translate(row["open"], *-origin),
            facecolor="#5d4739",
            edgecolor="none",
        )
    for record in plan["support_refinement"]["spaces"]:
        room = floor["rooms"][record["room_index"]]
        xy = np.asarray(room.representative_point().coords[0]) - origin
        axis.text(*xy, record["kind"], fontsize=8, ha="center", color="#263440")
    axis.set(
        xlim=(bounds[0] - origin[0] - 0.5, bounds[2] - origin[0] + 0.5),
        ylim=(bounds[1] - origin[1] - 0.5, bounds[3] - origin[1] + 0.5),
        aspect="equal",
        xlabel="East from source centre [m]",
        ylabel="North [m]",
        title=title + f" · {len(selection['selected'])}/2 doors",
    )
    axis.grid(alpha=0.15)


def run(output):
    figure, axes = plt.subplots(2, 3, figsize=(16, 9), layout="constrained")
    for column, (site, identifier) in enumerate(CONTROLS):
        _, original, _ = source_plan(site, identifier)
        old = refine(original)
        old_product = programme(old, "jittered-anchors")
        new = refine(original, door_aware=True)
        product, selection = joint(new)
        shapes = [
            level(plan, r["storey"])["rooms"][r["room_index"]]
            for plan in (old, new)
            for r in plan["support_refinement"]["spaces"]
        ]
        bounds = shapely.union_all(shapes).bounds
        origin = np.asarray(original["footprint"].centroid.coords[0])
        draw(
            axes[0, column],
            old,
            old_product,
            select(old, old_product),
            bounds,
            origin,
            site + " · post-fit",
        )
        draw(
            axes[1, column],
            new,
            product,
            selection,
            bounds,
            origin,
            site + " · reserve before placement",
        )
    figure.suptitle(
        "Actual source ground-floor support-zone crops · blue movement / green activity / brown furniture"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=120)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args().output)
