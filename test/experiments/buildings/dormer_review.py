"""Verify actual matched dormer rasters and complete retained city programmes."""

import argparse
import json
from collections import Counter
from pathlib import Path

from elevation_review import city_audit as elevations
from elevation_review import pair
from roof_crown_review import read, untimed


def city_audit(before, after):
    previous = elevations(before, after)
    previous.pop("plans")
    protected = (
        "facade_programme",
        "facade_architecture_plan",
        "roof_profile",
        "roof_kind",
        "roof_rise_m",
        "eave_m",
    )
    programmes = {}
    modes = Counter()
    for a, b in zip(
        before["source"]["members"], after["source"]["members"], strict=True
    ):
        for key in protected:
            if untimed(a["prototype"].get(key)) != untimed(b["prototype"].get(key)):
                raise ValueError("Protected roof/facade programme changed: " + key)
        plan = b["prototype"].get("roof_dormer_plan")
        if plan:
            programmes[b["id"]] = plan
            modes[b["prototype"]["roof_dormer_product"]] += 1
    for key in ("roof_crowns", "elevation_programmes"):
        if untimed(before["facades"][key]) != untimed(after["facades"][key]):
            raise ValueError("Previous actual exterior owner changed: " + key)
    return previous | {
        "dormer_roofs": len(programmes),
        "dormers": sum(len(p) for p in programmes.values()),
        "products": dict(modes),
        "rejected_sources": after["facades"]["dormer_rejected"],
        "retained_roof_crowns": len(after["facades"]["roof_crowns"]),
    }


def review(models, city):
    views = ("Near", "Opposite", "Elevated", "Far", "Detail")
    pairs = [pair(models, f"{i}{v}") for i in (0, 2) for v in views]
    for i in (0, 2):
        records = [read(models / f"{i}{v}After-geometry.json") for v in views]
        plans = [r["source_info"]["roof_dormer_plan"] for r in records]
        if any(p != plans[0] for p in plans):
            raise ValueError("Camera changed the logical source dormers")
        if records[3]["source_info"]["roof_dormer_product"] != "full":
            raise ValueError("Far control unexpectedly constructed near pockets")
        if records[4]["source_info"]["roof_dormer_product"] != "near":
            raise ValueError("Detail control omitted actual near pockets")
    pairs.append(pair(city, "ParisDormer"))
    return {
        "pairs": pairs,
        "city": city_audit(
            read(city / "ParisDormerBefore-geometry.json"),
            read(city / "ParisDormerAfter-geometry.json"),
        ),
        "scope": "Actual matched1920x1080 raster provenance and source/function records. RGB delta is not a quality score or native/device/cache/temporal admission. Optical inspection is separate.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", type=Path, required=True)
    parser.add_argument("--city", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(review(args.models, args.city), indent=2) + "\n")


if __name__ == "__main__":
    main()
