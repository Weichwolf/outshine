"""Rank compact real OSM windows by observed density and morphology, then inspect their plans."""

import argparse
import json
import math
from collections import Counter
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from city_survey_sources import CITIES, read
from evidence import revision
from matplotlib.collections import PatchCollection
from matplotlib.patches import PathPatch
from matplotlib.path import Path as PlotPath


def entropy(counts):
    values = np.asarray(counts, dtype=float)
    if values.sum() == 0:
        return 0.0
    weights = values[values > 0] / values.sum()
    return float(-(weights * np.log2(weights)).sum())


def patch(polygon):
    polygon = shapely.orient_polygons(polygon)
    rings = []
    for ring in (polygon.exterior, *polygon.interiors):
        codes = np.full(len(ring.coords), PlotPath.LINETO, dtype=np.uint8)
        codes[0], codes[-1] = PlotPath.MOVETO, PlotPath.CLOSEPOLY
        rings.append(PlotPath(np.asarray(ring.coords), codes))
    return PathPatch(PlotPath.make_compound_path(*rings))


def windows(buildings, pois):
    result = []
    for x in (-400, 0, 400):
        for y in (-400, 0, 400):
            selected = [
                r
                for r in buildings
                if abs(r["xy"][0] - x) <= 400 and abs(r["xy"][1] - y) <= 400
            ]
            area = np.histogram(
                [r["polygon"].area for r in selected],
                [0, 50, 100, 250, 500, 1500, 5000, np.inf],
            )[0]
            height = np.histogram(
                [r["height_m"] for r in selected],
                [0, 5, 10, 15, 25, 45, 80, 150, np.inf],
            )[0]
            shapes = Counter(r["shape"] for r in selected)
            semantic = Counter(
                str(
                    r["properties"].get(
                        "subclass", r["properties"].get("class", "unknown")
                    )
                )
                for r in pois
                if abs(r["xy"][0] - x) <= 400 and abs(r["xy"][1] - y) <= 400
            )
            diversity = entropy(area) + entropy(height) + entropy(list(shapes.values()))
            density = len(selected) / 0.64
            result.append(
                {
                    "centre_xy_m": [x, y],
                    "width_m": 800.0,
                    "building_polygons": len(selected),
                    "density_per_km2": density,
                    "area_histogram": area.tolist(),
                    "height_histogram": height.tolist(),
                    "shape_counts": dict(shapes),
                    "nearby_poi_classes": dict(semantic),
                    "score": math.log1p(density) * diversity,
                }
            )
    return sorted(result, key=lambda row: (-row["score"], row["centre_xy_m"]))


def run(output, fetch):
    output.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(2, 3, figsize=(19.2, 12.8), constrained_layout=True)
    reports = []
    for (city, centre), axis in zip(CITIES.items(), axes.flat, strict=False):
        buildings, pois, inputs = read(city, fetch)
        candidates = windows(buildings, pois)
        best = candidates[0]
        x, y = best["centre_xy_m"]
        selected = [
            r
            for r in buildings
            if abs(r["xy"][0] - x) <= 400 and abs(r["xy"][1] - y) <= 400
        ]
        patches, heights = [], []
        exports = []
        for row in sorted(selected, key=lambda item: (item["height_m"], item["id"])):
            patches.append(patch(row["polygon"]))
            heights.append(row["height_m"])
            exports.append(
                {k: v for k, v in row.items() if k != "polygon"}
                | {"footprint_wkb": row["polygon"].wkb_hex}
            )
        collection = PatchCollection(
            patches,
            array=np.asarray(heights),
            cmap="viridis",
            edgecolors="#28333d",
            linewidths=0.25,
        )
        collection.set_clim(0, 100)
        axis.add_collection(collection)
        axis.set(
            xlim=(x - 400, x + 400),
            ylim=(y - 400, y + 400),
            aspect="equal",
            title=f"{city}: {best['building_polygons']} polygons / 0.64 km²",
            xlabel="east [m]",
            ylabel="north [m]",
        )
        fig.colorbar(
            collection,
            ax=axis,
            shrink=0.7,
            label="provider top above flat datum [m], clipped at 100 m",
        )
        (output / (city + "-selected.json")).write_text(
            json.dumps(exports, indent=2) + "\n"
        )
        reports.append(
            {"city": city, "origin_lat_lon": centre, "windows": candidates, **inputs}
        )
    axes.flat[-1].axis("off")
    axes.flat[-1].text(
        0,
        0.9,
        "Preferred OSM vectors only; no DEM.\n800×800 m windows selected from a fixed local grid.\nScore: log(1+density) × summed area/height/shape entropy.\nClipped pieces excluded and explicitly recorded.\nPOI classes describe nearby uses, not whole-building functions.\nSelection heuristic, not a global diversity optimum.",
        va="top",
        fontsize=11,
    )
    fig.savefig(output / "city-survey.png", dpi=100)
    plt.close(fig)
    report = revision() | {
        "cities": reports,
        "ground_mode": "flat-no-dem",
        "scope": "Local provider census and visual scene selection; not a building reconstruction or architecture-family admission",
    }
    (output / "city-survey.json").write_text(json.dumps(report, indent=2) + "\n")
    for row in reports:
        best = row["windows"][0]
        print(
            row["city"],
            best["centre_xy_m"],
            best["building_polygons"],
            "polygons / 0.64 km²; score",
            round(best["score"], 3),
            "shapes",
            best["shape_counts"],
            flush=True,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fetch", action="store_true")
    args = parser.parse_args()
    run(args.output, args.fetch)
