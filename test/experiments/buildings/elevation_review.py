"""Actual raster provenance and retained source/function records for scoped facades."""

import argparse
import json
from collections import Counter
from pathlib import Path

from clad_facade_metrics import delta, pixels
from roof_crown_review import raster, read, untimed


def pair(root, name):
    before, first = raster(root, name + "Before")
    after, second = raster(root, name + "After")
    for key in (
        "lighting",
        "profile",
        "samples",
        "engine",
        "blender",
        "backend",
        "vendor",
    ):
        if first[key] != second[key]:
            raise ValueError("Matched render changed " + key)
    a, b = (
        read(root / (name + suffix + "-geometry.json"))
        for suffix in ("Before", "After")
    )
    if a["camera"] != b["camera"] or a.get("ground_span_m") != b.get("ground_span_m"):
        raise ValueError("Matched camera/ground declaration changed")
    return {
        "name": name,
        "before": str(before),
        "after": str(after),
        "before_geometry_commit": a["commit"],
        "after_geometry_commit": b["commit"],
        "rgb": delta(pixels(before), pixels(after)),
        "triangles": [a["triangles"], b["triangles"]],
    }


def city_audit(before, after):
    for key in ("camera", "sources", "window", "ground_span_m"):
        if before[key] != after[key]:
            raise ValueError("Whole window changed " + key)
    a, b = before["source"], after["source"]
    if (
        a["generated"] != b["generated"]
        or b["generated"] != 988
        or a["omissions"]
        or b["omissions"]
    ):
        raise ValueError("Complete source window was not retained")
    if a["material_definitions"] != b["material_definitions"]:
        raise ValueError("Base materials changed")
    protected = (
        "entrance_plan",
        "shopfront_plan",
        "balcony_plan",
        "window_bay_plan",
        "roof_crown_plan",
        "form_recipe",
        "room_ids",
    )
    plans = {}
    for old, new in zip(a["members"], b["members"], strict=True):
        if {k: v for k, v in old.items() if k != "prototype"} != {
            k: v for k, v in new.items() if k != "prototype"
        }:
            raise ValueError("Raw source fields changed: " + old["id"])
        for key in protected:
            if untimed(old["prototype"].get(key)) != untimed(new["prototype"].get(key)):
                raise ValueError("Protected programme changed: " + key)
        if "facade_architecture_plan" in new["prototype"]:
            expected = {
                p["facade_id"] + "/scope/" + s["name"]
                for p in new["prototype"]["facade_architecture_plan"]
                for s in p["scopes"]
            }
            actual = {
                f["id"]
                for f in new["prototype"].get("facade_plan", ())
                if "architecture_scope" in f
            }
            if expected != actual:
                raise ValueError(
                    "Unapplied source scopes advertised as current architecture"
                )
            plans[new["id"]] = new["prototype"]["facade_architecture_plan"]
    for key in ("podium_assets", "selected", "completed"):
        if untimed(before["procedures"][key]) != untimed(after["procedures"][key]):
            raise ValueError("Old mass/function record changed: " + key)
    counts = Counter(p["kind"] for values in plans.values() for p in values)
    return {
        "sources": 988,
        "omissions": 0,
        "changed_sources": len(plans),
        "elevations": sum(counts.values()),
        "kinds": dict(counts),
        "scopes": sum(len(p["scopes"]) for values in plans.values() for p in values),
        "retained_podium_assets": len(after["procedures"]["podium_assets"]),
        "retained_mass_function_records": len(after["procedures"]["completed"]),
        "source_ids_properties_footprints_heights_colours_placement_exact": True,
        "protected_entry_shop_projection_roof_programmes_equal_except_timing": True,
        "triangles": [before["triangles"], after["triangles"]],
        "plans": plans,
    }


def review(models, city, details=None):
    records = [
        pair(models, f"{index}{view}")
        for index in (0, 3)
        for view in ("Near", "Opposite", "Elevated", "Far")
    ]
    for index in (0, 3):
        source_plans = [
            read(models / f"{index}{v}After-geometry.json")["source_info"][
                "facade_architecture_plan"
            ]
            for v in ("Near", "Opposite", "Elevated", "Far")
        ]
        if any(p != source_plans[0] for p in source_plans):
            raise ValueError("Camera changed source architecture")
        if details:
            detail = read(details / f"{index}DetailAfter-geometry.json")["source_info"][
                "facade_architecture_plan"
            ]
            if detail != source_plans[0]:
                raise ValueError("Detail camera changed source architecture")
            records.append(pair(details, f"{index}Detail"))
    records.append(pair(city, "ParisElevation"))
    return {
        "pairs": records,
        "city": city_audit(
            read(city / "ParisElevationBefore-geometry.json"),
            read(city / "ParisElevationAfter-geometry.json"),
        ),
        "scope": "Actual matched1920×1080 RGBs and source/function records; pixel difference is not itself a quality score or native/device/cache/temporal admission.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("models", "city", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--details", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(review(args.models, args.city, args.details), indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
