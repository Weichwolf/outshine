"""Actual pixel comparisons and whole-window source/old-massing retention."""

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

import numpy as np
from PIL import Image
from clad_facade_metrics import delta, pixels


def read(path):
    return json.loads(path.read_text())


def raster(root, name):
    receipt = root / "renders" / (name + "-render.json")
    record = read(receipt)
    picture = receipt.parent / record["image"]
    assert hashlib.sha256(picture.read_bytes()).hexdigest() == record["image_sha256"]
    geometry = Path(record["geometry_report"])
    assert (
        hashlib.sha256(geometry.read_bytes()).hexdigest()
        == record["geometry_report_sha256"]
    )
    mesh = read(geometry)
    assert (
        hashlib.sha256(Path(mesh["mesh"]).read_bytes()).hexdigest()
        == record["mesh_sha256"]
    )
    with Image.open(picture) as image:
        assert image.size == (1920, 1080)
    return picture, record


def bounds(root, name, render):
    geometry = read(root / (name + "-geometry.json"))
    with np.load(geometry["mesh"]) as mesh:
        points = np.c_[mesh["vertices"], np.ones(len(mesh["vertices"]))]
    camera = render["camera"]
    clip = (
        points
        @ np.asarray(camera["view_matrix"]).T
        @ np.asarray(camera["projection_matrix"]).T
    )
    assert np.all(clip[:, 3] > 0)
    w, h = render["profile"]
    image = (clip[:, :2] / clip[:, 3:4] * [1, -1] + 1) * [w / 2, h / 2]
    low = np.maximum(np.floor(image.min(axis=0)).astype(int) - 1, 0)
    high = np.minimum(np.ceil(image.max(axis=0)).astype(int) + 1, [w - 1, h - 1])
    return [*low.tolist(), *high.tolist()]


def untimed(value):
    if isinstance(value, dict):
        return {k: untimed(v) for k, v in value.items() if not k.endswith("_ms")}
    if isinstance(value, list):
        return [untimed(v) for v in value]
    return value


def city_audit(before, after):
    for key in ("camera", "sources", "window", "facade_materials"):
        assert before[key] == after[key], key
    for key in ("material_definitions", "omissions"):
        assert before["source"][key] == after["source"][key], key
    assert before["source"]["generated"] == after["source"]["generated"] == 988
    assert not after["source"]["omissions"]
    for a, b in zip(
        before["source"]["members"], after["source"]["members"], strict=True
    ):
        assert {k: v for k, v in a.items() if k != "prototype"} == {
            k: v for k, v in b.items() if k != "prototype"
        }
    for key in ("podium_assets", "selected", "completed"):
        assert untimed(before["procedures"][key]) == untimed(
            after["procedures"][key]
        ), key
    crowns = [
        m["prototype"]["roof_crown_plan"]
        for m in after["source"]["members"]
        if "roof_crown_plan" in m["prototype"]
    ]
    return {
        "sources": 988,
        "zero_omissions": True,
        "raw_source_fields_exact": True,
        "old_massing_function_records_equal_except_timing": len(
            after["procedures"]["completed"]
        ),
        "retained_podium_assets": len(after["procedures"]["podium_assets"]),
        "crowns": dict(Counter(p["kind"] for p in crowns)),
        "products": dict(Counter(p["product"] for p in after["roof_crowns"])),
        "triangles_before": before["triangles"],
        "triangles_after": after["triangles"],
    }


def review(models, city, old_city=None):
    records = []
    for index in (0, 3):
        full, render = raster(models, f"{index}FarFull")
        coarse, _record = raster(models, f"{index}FarAfter")
        bbox = bounds(models, f"{index}FarFull", render)
        near, near_render = raster(models, f"{index}NearAfter")
        counter, _record = raster(models, f"{index}NearCoarse")
        near_box = bounds(models, f"{index}NearAfter", near_render)
        records.append(
            {
                "index": index,
                "independent_projected_bounds": bbox,
                "selected_error_px": read(models / f"{index}FarAfter-geometry.json")[
                    "source_info"
                ]["roof_projected_error_px"],
                "far_global": delta(pixels(full), pixels(coarse)),
                "far_roi": delta(pixels(full), pixels(coarse), bbox),
                "near_forced_coarse_roi": delta(
                    pixels(near), pixels(counter), near_box
                ),
            }
        )
    before = read(city / "ParisCrownBefore-geometry.json")
    after = read(city / "ParisCrownAfter-geometry.json")
    original, _record = (
        raster(old_city, "ParisCladAfter")
        if old_city is not None
        else raster(city, "ParisCrownBefore")
    )
    changed, _record = raster(city, "ParisCrownAfter")
    return {
        "city": city_audit(before, after),
        "views": records,
        "city_pixel_delta": delta(pixels(original), pixels(changed)),
        "scope": "Actual RGB bytes; ROI comes from complete mesh/camera bounds, not observed differences. Daylight software Eevee16 only; not a universal radiance/shadow/night/temporal/native/device bound. Old city raster requires the separate exact numeric input/camera/material replay proof.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("models", "city", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--old-city", type=Path)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        parser.error("--output must be inside /tmp")
    result = review(args.models, args.city, args.old_city)
    args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
