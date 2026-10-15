"""First 2D source-room programmes: actual objects, activity areas and portal routes."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_plan_views import patch
from building_vertical import build, level
from evidence import revision
from room_programme import programme
from shapely.affinity import translate
from tile_buildings import read_tile
from tile_facades import contexts
from tile_room_details import numeric_properties

CONTROLS = (
    ("BerlinFernsehturm", "BerlinFernsehturm/14/8802/5373/building/38981530/6/195"),
    ("Hameln", "Hameln/14/8617/5404/building/17363470/3/848"),
    ("Spittelau", "Spittelau/14/8936/5679/building/16961800/9/23"),
)


def source_plan(site, identifier):
    rows, receipt = read_tile(site)
    contexts(rows, receipt)
    row = next(r for r in rows if r["id"] == identifier)
    plan = build(
        {
            "id": row["id"],
            "polygon": row["polygon"],
            "properties": numeric_properties(row),
            "context": row.get("context", {}),
        }
    )
    if plan.get("vertical_status") != "connected-storeys":
        raise ValueError("Actual source has no connected room plan")
    return row, plan, receipt


def render(output):
    output.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(1, len(CONTROLS), figsize=(24, 13.5))
    records = []
    for axis, (site, identifier) in zip(axes, CONTROLS, strict=True):
        _row, plan, _receipt = source_plan(site, identifier)
        product = programme(plan, "jittered-anchors")
        origin = np.asarray(plan["footprint"].centroid.coords[0])
        current = level(plan, 0)
        patch(
            axis,
            translate(current["footprint"], *-origin),
            facecolor="#525d68",
            edgecolor="none",
        )
        for index, room in enumerate(current["rooms"]):
            patch(
                axis, translate(room, *-origin), facecolor="#edf0ef", edgecolor="none"
            )
        for room in product["rooms"]:
            point = (
                np.asarray(
                    current["rooms"][room["room_index"]]
                    .representative_point()
                    .coords[0]
                )
                - origin
            )
            axis.text(
                *point,
                room["kind"],
                fontsize=10,
                ha="center",
                va="center",
                color="#34445c",
            )
            for item in room["result"]["items"]:
                patch(
                    axis,
                    translate(item["activity"], *-origin),
                    facecolor="#c5daba",
                    edgecolor="none",
                    alpha=0.55,
                )
                patch(
                    axis,
                    translate(item["footprint"], *-origin),
                    facecolor="#b6905c",
                    edgecolor="#684c2d",
                    linewidth=0.6,
                )
            for line in room["result"]["proof"]["paths"]:
                xy = np.asarray(line.coords) - origin
                axis.plot(xy[:, 0], xy[:, 1], color="#2a7c72", lw=1.2)
        for door in current["doors"]:
            xy = np.asarray(door["centre"]) - origin
            axis.plot(*xy, "o", ms=4, color="#bf7032")
        axis.set(
            aspect="equal",
            xlabel="Ost ab Gebäudezentrum [m]",
            ylabel="Nord [m]",
            title=site + " · " + product["status"],
        )
        axis.autoscale_view()
        axis.grid(alpha=0.15)
        records.append(
            {
                "id": identifier,
                "status": product["status"],
                "evidence": product["evidence"],
                "unresolved": product["unresolved"],
                "rooms": [
                    {
                        "id": r["room_id"],
                        "kind": r["kind"],
                        "status": r["result"]["status"],
                        "items": len(r["result"]["items"]),
                        "missing": r["result"]["missing"],
                        "reason": r["result"].get("reason"),
                        "tested": r["result"]["tested"],
                    }
                    for r in product["rooms"]
                ],
            }
        )
    fig.suptitle(
        "Quellgebundene Büro-Nutzungsbereiche · Gegenstände, Aktivitätsflächen und freie Wege",
        fontsize=22,
    )
    fig.text(
        0.05,
        0.04,
        "Braun: vollständige Möbelgrundrisse. Grün: benötigte Aktivitätsflächen. Türkis: geprüfte Wege von realen Portalen.\n"
        "Büro-POIs bleiben Teilnutzung; fehlende Etage wird nur ausdrücklich als Erdgeschoss angenommen. Leitungen, Lüftung, Vorschriften und native Begehbarkeit bleiben offen.",
        fontsize=14,
    )
    fig.tight_layout(rect=(0.0, 0.12, 1.0, 0.94))
    path = output / "room-programme-plan.png"
    fig.savefig(path, dpi=120)
    plt.close(fig)
    (output / "room-programme-plan.json").write_text(
        json.dumps(revision() | {"profile": [2880, 1620], "cases": records}, indent=2)
        + "\n"
    )
    print(path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/room-programme"),
    )
    render(parser.parse_args().output)
