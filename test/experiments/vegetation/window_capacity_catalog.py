"""Finite physical size classes for existing partial needle windows."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from conifer_whole_products import load_catalog, save_catalog
from conifer_window_catalog import family_geometry_for_seed, prototypes, template_image
from conifer_windows import pattern_key
from foliage_selection import directions
from outshine_ref.vegetation import NeedlePattern


def build(original, output):
    began = time.perf_counter()
    code = Path(__file__).resolve()
    if subprocess.check_output(["git", "status", "--porcelain", "--", str(code)]):
        raise ValueError("freeze finite size-class implementation first")
    metadata, banks = load_catalog(original / "catalog")
    source = json.loads((original / "catalog/source.json").read_text())
    if len(metadata) != 1 or source["version"] != "canonical-spruce-inputs-1":
        raise ValueError("explicit own single-pattern spruce source required")
    key, info = next(iter(metadata.items()))
    pattern = NeedlePattern.from_recipe(info["pattern"])
    if info["needles"] != 256 or pattern.group_size != 1:
        raise ValueError("bounded256group source with single needles required")
    models = original / "models"
    meshes, paths = prototypes(models, json.loads((models / "shoot.json").read_text()))
    output.mkdir(parents=True, exist_ok=False)
    families = {key: (None, info)}
    generated = 0
    for groups in (128, 64, 32, 16, 8, 4, 2):
        if groups * (len(meshes[0][0]["faces"]) + 36) > 250000:
            raise ValueError("source triangle budget before canonical expansion")
        geometry, facts = family_geometry_for_seed(
            source["canonical_seed"], pattern, meshes, groups
        )
        facts["family"] = len(families)
        if facts["phase"] != info["phase"]:
            raise ValueError("same canonical phase before selecting size classes")
        banks.append([template_image(geometry, d, 32, 8) for d in directions(64)])
        families[json.dumps({"pattern": key, "groups": groups}, sort_keys=True)] = (
            geometry,
            facts,
        )
        generated += facts["needles"]
    save_catalog(output, families, banks)
    inputs = [
        original / "catalog/source.json",
        original / "catalog/catalog.json",
        original / "catalog/family-0.npz",
        models / "shoot.json",
        *paths,
    ]
    receipt = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "implementation_sha256": {
            str(code): hashlib.sha256(code.read_bytes()).hexdigest()
        },
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs
        },
        "reused_full_family": True,
        "generated_canonical_needles": generated,
        "families": len(families),
        "decoded_image_bytes": sum(image.nbytes for bank in banks for image, _ in bank),
        "seconds": time.perf_counter() - began,
    }
    (output / "source.json").write_text(
        json.dumps(
            source
            | {
                "version": "finite-window-capacities-1",
                "size_classes": [2, 4, 8, 16, 32, 64, 128, 256],
                "construction": receipt,
            },
            indent=2,
        )
        + "\n"
    )
    (output / "capacity-report.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(
        json.dumps(
            receipt
            | {
                "input_sha256": "recorded in file",
                "implementation_sha256": "recorded in file",
            }
        ),
        flush=True,
    )


def rebind(plan, original, capacities):
    result = dict(plan)
    families = plan["families"].copy()
    counts = plan["capacities"].copy()
    for old in original.values():
        key = pattern_key(NeedlePattern.from_recipe(old["pattern"]))
        choices = sorted(
            (
                r
                for r in capacities.values()
                if pattern_key(NeedlePattern.from_recipe(r["pattern"])) == key
            ),
            key=lambda r: r["needles"],
        )
        if (
            not choices
            or choices[-1]["needles"] != old["needles"]
            or len({c["needles"] for c in choices}) != len(choices)
            or any(c["phase"] != old["phase"] for c in choices)
        ):
            raise ValueError(
                "same physical pattern/maximum capacity/canonical phase required"
            )
        mask = plan["families"] == old["family"]
        need = plan["needle_counts"][mask]
        sizes = np.array([c["needles"] for c in choices], np.uint32)
        if np.any(need == 0) or np.any(need > sizes[-1]):
            raise ValueError("bounded positive existing needle windows required")
        chosen = np.searchsorted(sizes, need)
        counts[mask] = sizes[chosen]
        families[mask] = np.array([c["family"] for c in choices], np.int32)[chosen]
    result |= {
        "families": families,
        "capacities": counts,
        "weights": plan["needle_counts"] / counts,
    }
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.source, args.output)
