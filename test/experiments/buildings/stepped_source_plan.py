"""Actual source storey contours, rooms and terrace portals before 3D rendering."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_plan import wall_at
from building_plan_views import patch
from evidence import revision
from stepped_source import plan_for
from tile_buildings import read_tile


def render(output):
    output.mkdir(parents=True, exist_ok=True)
    rows, _ = read_tile("Hameln")
    row = next(
        r for r in rows if r["id"] == "Hameln/14/8617/5404/building/1267876880/11/0"
    )
    plan = plan_for(row)
    fig, axes = plt.subplots(1, 3, figsize=(24, 13.5))
    origin = np.asarray(plan["footprint"].centroid.coords[0])
    records = []
    from shapely.affinity import translate

    for axis, index in zip(
        axes, (0, plan["storeys"] - 2, plan["storeys"] - 1), strict=True
    ):
        current = plan["storey_plans"][index]
        for region, colour in (
            (plan["footprint"], "#eef1f3"),
            (current["terrace"], "#e8c587"),
            (current["terrace_guard"], "#a58e70"),
            (wall_at(current, current["floor_z_m"] + 0.4), "#535e69"),
        ):
            patch(axis, translate(region, *-origin), facecolor=colour, edgecolor="none")
        for i, room in enumerate(current["rooms"]):
            centre = np.asarray(room.representative_point().coords[0]) - origin
            axis.text(*centre, str(i), ha="center", fontsize=10)
        for door in current["doors"]:
            point = np.asarray(door["centre"]) - origin
            axis.plot(*point, "o", color="#c67b32", ms=5)
        axis.set(
            aspect="equal",
            xlabel="Ost ab Gebäudezentrum [m]",
            ylabel="Nord [m]",
            title=f"Geschoss {index + 1} · {current['footprint'].area:.1f}m² · Boden {current['floor_z_m']:.2f}m",
        )
        axis.autoscale_view()
        axis.grid(alpha=0.15)
        records.append(
            dict(
                storey=index,
                footprint_m2=current["footprint"].area,
                terrace_m2=current["terrace"].area,
                rooms=len(current["rooms"]),
                doors=len(current["doors"]),
            )
        )
    fig.suptitle(
        "Hameln · tatsächliches Staffelgebäude mit geschütztem Kern und verbundenen Terrassen",
        fontsize=23,
    )
    fig.text(
        0.04,
        0.04,
        "Grau: wirkliche Wandquerschnitte. Ocker: freigelegte Terrassen/Brüstungen. Goldpunkte: echte Türportale.\n"
        "Quellgrundriss am Boden und Gesamtoberkante bleiben erhalten. Geschossformen/Räume sind prozedural; Nutzungsprogramm und native Begehbarkeit bleiben offen.",
        fontsize=15,
    )
    fig.tight_layout(rect=(0.0, 0.13, 1.0, 0.93))
    path = output / "stepped-source-plan.png"
    fig.savefig(path, dpi=120)
    plt.close(fig)
    (output / "stepped-source-plan.json").write_text(
        json.dumps(
            revision()
            | dict(
                id=row["id"],
                profile=[2880, 1620],
                cases=records,
                form_recipe=plan["form_recipe"],
            ),
            indent=2,
        )
        + "\n"
    )
    print(path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/stepped-source"),
    )
    render(parser.parse_args().output)
