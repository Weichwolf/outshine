"""Entire source window and actual candidate storey profiles in2D; no 3D admission."""

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_plan_views import patch
from city_scene import inputs
from evidence import revision
from terraced_mass_census import source_identity
from terraced_mass_plan import plan


def render(report, output):
    root = Path(__file__).with_name("data")
    rows, receipts, _window = inputs(
        "Paris", json.loads((root / "roof-crown-city-window.json").read_text())
    )
    expected = sorted((r["url"], r["sha256"]) for r in report["sources"]["receipts"])
    if sorted((r["url"], r["sha256"]) for r in receipts["receipts"]) != expected:
        raise ValueError("Plan map source receipts changed")
    buildings = {r["id"]: r for r in rows if r["layer"] == "building"}
    if set(buildings) != {r["id"] for r in report["records"]}:
        raise ValueError("Plan map lost an original source")
    figure, axes = plt.subplots(1, 2, figsize=(18, 10.125), dpi=160)
    colours = {"cascade": "#cb8658", "paired-wings": "#5893ac"}
    counts = {"sources": 0, "candidates": 0, "unique_profiles": 0, "protected": 0}
    all_xy = np.vstack(
        [shapely.get_coordinates(r["polygon"]) for r in buildings.values()]
    )
    lo, hi = all_xy.min(axis=0) - 10, all_xy.max(axis=0) + 10
    for record in report["records"]:
        row = buildings[record["id"]]
        if any(record[k] != v for k, v in source_identity(row).items()):
            raise ValueError("Plan map original attributes or geometry changed")
        for axis in axes:
            patch(
                axis,
                row["polygon"],
                facecolor="#e3e5e6",
                edgecolor="#9ba1a5",
                linewidth=0.3,
            )
        counts["sources"] += 1
        if record["status"] == "retained-existing-functional-owner":
            patch(
                axes[1],
                row["polygon"],
                facecolor="#afd0b5",
                edgecolor="#44805c",
                linewidth=0.55,
            )
            counts["protected"] += 1
        if record["status"] != "direct-candidate":
            continue
        mass = plan(row, record["programme"])
        profiles = tuple(dict.fromkeys(s.footprint_wkb for s in mass.storeys))
        for i, wkb in enumerate(profiles):
            patch(
                axes[1],
                shapely.from_wkb(wkb),
                facecolor=colours[mass.kind],
                edgecolor="#33464d",
                alpha=0.35 + 0.2 * i,
                linewidth=0.5,
            )
        counts["candidates"] += 1
        counts["unique_profiles"] += len(profiles)
    for axis, title in zip(
        axes,
        (
            f"{counts['sources']} ursprüngliche Gebäudegrundrisse",
            f"{counts['candidates']} direkte Kandidaten: tatsächliche Staffel-/Flügelprofile",
        ),
        strict=True,
    ):
        axis.set(
            xlim=(lo[0], hi[0]),
            ylim=(lo[1], hi[1]),
            aspect="equal",
            title=title,
            xlabel="Ost [m]",
            ylabel="Nord [m]",
        )
        axis.set_facecolor("#fafaf8")
    figure.suptitle(
        "Ganze feste Quellauswahl · gleiche Ausdehnung · Höfe bleiben offen",
        fontsize=15,
    )
    figure.text(
        0.5,
        0.055,
        f"Orange: Kaskade · Blau: obere Flügel · dunkler: höhere Profile · Grün: {counts['protected']} bestehende Funktionsbesitzer (nur Lage markiert)",
        ha="center",
        fontsize=10,
    )
    figure.text(
        0.5,
        0.028,
        "2D-Forschungsstand: neue Kandidaten noch ohne vollständige Begehbarkeits-/Stadtbild-/Runtime-Abnahme. Keine DEM-Anpassung.",
        ha="center",
        fontsize=10,
    )
    figure.subplots_adjust(left=0.055, right=0.98, bottom=0.12, top=0.90, wspace=0.12)
    output.mkdir(parents=True, exist_ok=True)
    path = output / "whole-source-plans.png"
    figure.savefig(path, dpi=160)
    plt.close(figure)
    summary = report["summary"]
    if (counts["sources"], counts["candidates"], counts["protected"]) != (
        summary["source_buildings"],
        summary["status_counts"]["direct-candidate"],
        summary["protected_functional_owners"],
    ):
        raise ValueError("Plan map differs from the current candidate census")
    return {
        "image": str(path),
        "image_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "profile": [2880, 1620],
        "counts": counts,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve() == Path(
        "/tmp"
    ) or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a subdirectory of /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the plan map producer first")
    source = json.loads(args.census.read_text())
    if source["experiment_tree_dirty"]:
        parser.error("Plan map requires a frozen source census")
    result = render(source, args.output)
    (args.output / "plan-map.json").write_text(
        json.dumps(
            proof
            | result
            | {
                "census_commit": source["commit"],
                "census_sha256": hashlib.sha256(args.census.read_bytes()).hexdigest(),
                "scope": "Actual all-source2D plan inspection; no replaced source/DEM/native/3D/closed-volume/access admission.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps(result))


if __name__ == "__main__":
    main()
