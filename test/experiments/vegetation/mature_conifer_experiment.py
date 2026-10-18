"""Bake eight complete mature conifer forms from the existing finite catalog."""

import argparse
import json
from pathlib import Path

from conifer_whole_products import BASE, build_one, prepare_catalog
from mature_conifer_recipes import prepare
from mature_density import MAX_AXES, MAX_WINDOWS, WINDOW_SETTINGS


def build(output, size=256, *, plans_only=False, phase="all"):
    cases = prepare(output / "recipes")
    for case in cases:
        summary = case["summary"]
        if summary["axes"] > MAX_AXES or summary["windows"] > MAX_WINDOWS:
            raise ValueError(
                "complete mature form exceeds its declared experiment budget"
            )
    if plans_only:
        return cases
    if phase in ("all", "catalog"):
        for species in dict.fromkeys(case["species"] for case in cases):
            prepare_catalog(species, output / "catalogs", **WINDOW_SETTINGS, samples=8)
    if phase == "catalog":
        return cases
    for case in cases:
        result = build_one(
            case["key"],
            output / "products",
            size,
            catalog_directory=output / "catalogs" / case["species"] / "catalog",
            recipe_path=Path(case["recipe"]),
            bark_path=BASE / case["species"] / "bark.json",
            max_windows=MAX_WINDOWS,
            max_axes=MAX_AXES,
        )
        assert result["plan_identity"] == case["identity"]
        case["product"] = str(Path("products") / case["key"])
    (output / "mature-products.json").write_text(json.dumps(cases, indent=2) + "\n")
    return cases


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/vegetation/mature-conifers"),
    )
    parser.add_argument("--size", type=int, choices=(128, 256), default=256)
    parser.add_argument("--plans-only", action="store_true")
    parser.add_argument(
        "--phase", choices=("all", "catalog", "products"), default="all"
    )
    args = parser.parse_args()
    build(args.output, args.size, plans_only=args.plans_only, phase=args.phase)
