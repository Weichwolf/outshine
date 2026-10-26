"""Private source-bound 2D door sweeps against current furniture and activities."""

import argparse
import json
from dataclasses import asdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_plan_views import patch
from building_vertical import level
from door_support_plan import choices
from evidence import revision
from office_support_plan import refine
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan
from shapely.affinity import translate


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(2, 3, figsize=(24, 13.5))
    records = []
    for column, (site, identifier) in enumerate(CONTROLS):
        _row, original, receipt = source_plan(site, identifier)
        plan = refine(original)
        product = programme(plan, "jittered-anchors")
        for row, method in enumerate(("single-hull", "segmented")):
            axis = axes[row, column]
            current = level(plan, 0)
            origin = np.asarray(plan["footprint"].centroid.coords[0])
            patch(
                axis,
                translate(current["footprint"], *-origin),
                facecolor="#4e5965",
                edgecolor="none",
            )
            for room in current["rooms"]:
                patch(
                    axis,
                    translate(room, *-origin),
                    facecolor="#eef0ee",
                    edgecolor="none",
                )
            leaves = choices(plan, product, method)
            for item in leaves:
                if item["storey"]:
                    continue
                colour = "#65ab86" if item["fits_existing"] else "#d89282"
                patch(
                    axis,
                    translate(item["motion"], *-origin),
                    facecolor=colour,
                    edgecolor=colour,
                    alpha=0.35,
                )
                patch(
                    axis,
                    translate(item["open"], *-origin),
                    facecolor="#735236",
                    edgecolor="none",
                )
                point = item["leaf"].hinge - origin
                axis.plot(*point, "o", color="#513f31", markersize=3)
            for room in product["rooms"]:
                for item in room["result"]["items"]:
                    patch(
                        axis,
                        translate(item["activity"], *-origin),
                        facecolor="#c6dcb9",
                        edgecolor="none",
                        alpha=0.5,
                    )
                    patch(
                        axis,
                        translate(item["footprint"], *-origin),
                        facecolor="#ba935e",
                        edgecolor="#765936",
                        lw=0.4,
                    )
            axis.set(
                aspect="equal",
                title=site + " · " + method,
                xlabel="Ost ab Gebäudezentrum [m]",
                ylabel="Nord [m]",
            )
            axis.autoscale_view()
            axis.grid(alpha=0.1)
            records.append(
                {
                    "id": identifier,
                    "receipt": receipt,
                    "method": method,
                    "candidates": [
                        {
                            k: v
                            for k, v in item.items()
                            if k not in ("leaf", "motion", "open", "closed")
                        }
                        | {
                            "leaf": asdict(item["leaf"]),
                            "motion_wkb": item["motion"].wkb_hex,
                        }
                        for item in leaves
                    ],
                }
            )
    fig.suptitle(
        "Nach innen öffnende Nebenraumtüren · konservative Schwenkhüllen und bestehende Einrichtung",
        fontsize=20,
    )
    fig.text(
        0.04,
        0.02,
        "Braun: Möbel und geparkte Türblätter · Hellgrün: Aktivitätsflächen · Grün/Rot: passende/kollidierende Schwenkhülle.\n"
        "Beide Bandseiten werden geprüft. Diese Diagnose verschiebt weder Möbel noch Quellen; eine fehlende passende Tür bleibt offen.",
        fontsize=12,
    )
    fig.tight_layout(rect=(0, 0.08, 1, 0.95))
    fig.savefig(output / "door-sweep-plan.png", dpi=120)
    plt.close(fig)
    (output / "door-sweep-plan.json").write_text(
        json.dumps(
            revision()
            | {"status": "2d-diagnostic", "profile": [2880, 1620], "cases": records},
            indent=2,
        )
        + "\n"
    )
    print(
        [
            (
                r["id"].split("/")[0],
                r["method"],
                sum(v["fits_existing"] for v in r["candidates"]),
                len(r["candidates"]),
            )
            for r in records
        ]
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)
