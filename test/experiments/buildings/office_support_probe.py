"""Private 2D before/after plans of actual provider office controls."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_plan_views import patch
from building_vertical import level
from evidence import revision
from office_support_plan import refine
from office_support_verification import admission
from room_programme import room_kinds
from room_programme_plan import CONTROLS, source_plan
from shapely.affinity import translate

COLOURS = {
    "sanitary": "#d6c0d9",
    "tea": "#efd4a8",
    "meeting": "#bdd4e6",
    "work": "#d4e4cf",
    "work-tea": "#d4e4cf",
    "archive": "#dfded4",
}


def draw(axis, plan, title):
    current = level(plan, 0)
    origin = np.asarray(plan["footprint"].centroid.coords[0])
    eligible = [
        i for i, kind in enumerate(current["room_kinds"]) if kind == "generic-room"
    ]
    daylight = {i for i in eligible if any(i in w["rooms"] for w in current["windows"])}
    kinds = room_kinds(current, eligible, daylight)
    patch(
        axis,
        translate(current["footprint"], *-origin),
        facecolor="#343d47",
        edgecolor="none",
    )
    for index, room in enumerate(current["rooms"]):
        kind = kinds.get(index, current["room_kinds"][index])
        patch(
            axis,
            translate(room, *-origin),
            facecolor=COLOURS.get(kind, "#e7ebee"),
            edgecolor="none",
        )
        point = np.asarray(room.representative_point().coords[0]) - origin
        axis.text(
            *point, f"{kind}\n{room.area:.1f} m²", fontsize=8, ha="center", va="center"
        )
    for door in current["doors"]:
        patch(
            axis,
            translate(door["polygon"], *-origin),
            facecolor="#c77948",
            edgecolor="none",
        )
    for window in current["windows"]:
        patch(
            axis,
            translate(window["polygon"], *-origin),
            facecolor="#58a6bb",
            edgecolor="none",
        )
    low = np.asarray(plan["footprint"].bounds[:2]) - origin - 1.0
    high = np.asarray(plan["footprint"].bounds[2:]) - origin + 1.0
    axis.set(
        xlim=(low[0], high[0]),
        ylim=(low[1], high[1]),
        aspect="equal",
        title=title,
        xlabel="Ost ab Gebäudezentrum [m]",
        ylabel="Nord [m]",
    )
    axis.grid(alpha=0.12)
    return {
        i: {"kind": kind, "area_m2": current["rooms"][i].area}
        for i, kind in kinds.items()
    }


def probe(output):
    output.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(3, 3, figsize=(24, 18))
    records = []
    for column, (site, identifier) in enumerate(CONTROLS):
        row, original, receipt = source_plan(site, identifier)
        before = draw(axes[0, column], original, site + " · bisher")
        for index, method in enumerate(("wall-ends", "sliding-wall"), 1):
            plan = refine(original, method)
            proof = admission(original, plan)
            summary = plan["support_refinement"]
            after = draw(axes[index, column], plan, site + " · " + method)
            records.append(
                {
                    "id": identifier,
                    "source_properties": row["properties"],
                    "receipt": receipt,
                    "method": method,
                    "before": before,
                    "after": after,
                    "refinement": summary,
                    "plan_proof": proof,
                    "room_ids": plan["storey_room_ids"],
                    "source_footprint_wkb": original["footprint"].wkb_hex,
                    "floor_rooms_wkb": [r.wkb_hex for r in level(plan, 0)["rooms"]],
                }
            )
    fig.suptitle(
        "Büro-Nebenräume vor der Einrichtung · ganze unveränderte Gebäudegrundrisse",
        fontsize=20,
    )
    fig.text(
        0.04,
        0.02,
        "Beige: Teeküche · Lila: Sanitär · Blau: Besprechung · Grün: Arbeit · Orange: tatsächliche Türöffnungen.\n"
        "Neue Nebenräume haben unabhängigen Flurzugang; ursprüngliche Raum-IDs bleiben Eltern. Fehlende Kandidaten bleiben offen.\n"
        "2D-Entwurf: Möblierung, Türblätter/Privatheitszustände, Lüftung, 3D- und native Abnahme folgen getrennt.",
        fontsize=12,
    )
    fig.tight_layout(rect=(0.0, 0.08, 1.0, 0.95))
    path = output / "office-support-plan.png"
    fig.savefig(path, dpi=120)
    plt.close(fig)
    result = revision() | {
        "status": "2d-research",
        "profile": [2880, 2160],
        "controls": records,
    }
    (output / "office-support-plan.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                "output": str(path),
                "cases": len(records),
                "support_rooms": [len(r["refinement"]["spaces"]) for r in records],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/office-space-plan"),
    )
    args = parser.parse_args()
    probe(args.output)
