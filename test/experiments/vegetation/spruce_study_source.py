"""Rebuild own fixed spruce inputs without a private cache or PlantPlan-v4 parser."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from conifer_whole_products import save_catalog
from conifer_window_catalog import family_geometry_for_seed, prototypes, template_image
from conifer_windows import pattern_key
from foliage_selection import directions
from needle_material import write_needle_material
from outshine_ref.vegetation import Needle, NeedlePattern
from parent_profile_graph import ParentProfileGraph
from upper_crown_profiles import profiles

INPUT = Path(__file__).with_name("data") / "picea-mature-study.json"


def read_input(path):
    data = json.loads(path.read_text())
    if (
        data["version"] != "spruce-study-input-1"
        or data["physical_source"]["version"] != "physical-needle-source-1"
    ):
        raise ValueError("explicit own study/physical-source versions required")
    settings = data["catalog"]
    if (
        type(settings["seed"]) is not int
        or not 0 <= settings["seed"] < 2**32
        or type(settings["groups"]) is not int
        or not 1 <= settings["groups"] <= 256
        or settings["directions"] != 64
        or settings["size"] != 32
        or type(settings["subpixel_samples"]) is not int
        or not 1 <= settings["subpixel_samples"] <= 8
        or not 1 <= len(data["physical_source"]["needles"]) <= 8
        or len(data["physical_source"]["patterns"]) != 4
    ):
        raise ValueError("bounded explicit canonical input before any generation")
    graph = profiles(data["base_graph"], **data["upper_profile"])
    if (
        ParentProfileGraph.from_recipe(graph).identity
        != data["reference_graph_identity"]
    ):
        raise ValueError("fixed source graph must reproduce its declared identity")
    return data, graph


def field_hashes(values):
    return {
        name: {
            "shape": list(a.shape),
            "dtype": a.dtype.str,
            "sha256": hashlib.sha256(a.tobytes()).hexdigest(),
        }
        for name, a in values.items()
    }


def binding_metadata(metadata):
    # Attachment first/last vary by tree order; canonical families override them.
    return {
        key: info | {"pattern": pattern_key(NeedlePattern.from_recipe(info["pattern"]))}
        for key, info in metadata.items()
    }


def provenance(path):
    root = Path(__file__).resolve().parents[3]
    files = [
        Path(__file__).with_name(name)
        for name in (
            "spruce_study_source.py",
            "conifer_window_catalog.py",
            "conifer_whole_products.py",
            "needle_material.py",
            "needle_group_source.py",
            "raster_batch.py",
            "plant_atlas.py",
            "leaf_sheet.py",
            "upper_crown_profiles.py",
        )
    ]
    files.append(path)
    return {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", *map(str, files)],
                cwd=root,
                text=True,
            ).strip()
        ),
        "implementation_sha256": {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in files
            if p.is_relative_to(root)
        },
    }


def create_source(path, output, *, verify_reference=False):
    start = time.perf_counter()
    data, graph = read_input(path)
    physical, settings = data["physical_source"], data["catalog"]
    output.mkdir(parents=True, exist_ok=False)
    models = output / "models"
    models.mkdir()
    material = write_needle_material(
        models, data["material"]["section"], base_colour=data["material"]["base_colour"]
    )
    products, mesh_hashes = [], []
    for index, parameters in enumerate(physical["needles"]):
        mesh = Needle(**parameters).mesh(**data["needle_mesh"])
        mesh_hashes.append(field_hashes(mesh))
        name = f"needle-{index}.npz"
        np.savez_compressed(models / name, **mesh)
        products.append({"surface": "leaf", "mesh": name, "needle_material": material})
    scene = {"version": "needle-prototype-source-1", "products": products}
    (models / "shoot.json").write_text(json.dumps(scene, indent=2) + "\n")
    meshes, _ = prototypes(models, scene)
    families, banks = {}, []
    for value in physical["patterns"]:
        if value is None:
            continue
        pattern = NeedlePattern.from_recipe(value)
        key = pattern_key(pattern)
        if key in families:
            continue
        triangle_bound = settings["groups"] * (
            pattern.group_size * max(len(m["faces"]) for m, _ in meshes)
            + (36 if pattern.short_length else 0)
        )
        if triangle_bound > 1_000_000:
            raise ValueError("finite family triangle budget before organ expansion")
        source, info = family_geometry_for_seed(
            settings["seed"], pattern, meshes, settings["groups"]
        )
        info["family"] = len(families)
        bank = [
            template_image(
                source, direction, settings["size"], settings["subpixel_samples"]
            )
            for direction in directions(settings["directions"])
        ]
        families[key] = (source, info)
        banks.append(bank)
    save_catalog(output / "catalog", families, banks)
    with np.load(output / "catalog/family-0.npz", allow_pickle=False) as saved:
        family_hashes = field_hashes({k: saved[k] for k in saved.files})
    metadata = json.loads((output / "catalog/catalog.json").read_text())
    if verify_reference and (
        mesh_hashes != data["reference_fields"]["needle_meshes"]
        or family_hashes != data["reference_fields"]["family"]
        or binding_metadata(metadata)
        != binding_metadata(data["reference_catalog_metadata"])
    ):
        raise ValueError("same-host legacy decoded fields must remain exact")
    recipe = {
        "version": "experimental-parent-needled-2",
        "wood": graph,
        "physical_source": physical,
        "authoring": data["authoring"],
    }
    (output / "plant-recipe.json").write_text(json.dumps(recipe, indent=2) + "\n")
    report = provenance(path.resolve()) | {
        "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "graph_identity": data["reference_graph_identity"],
        "plant_seed": graph["seed"],
        "catalog_seed": settings["seed"],
        "reference_fields_verified": verify_reference,
        "needle_fields": mesh_hashes,
        "family_fields": family_hashes,
        "build_s": time.perf_counter() - start,
        "scope": "Own fixed recipes and procedural material; three physical needle meshes and finite64view catalog only, no whole plant/needle expansion or private predecessor required. Hash reference is same-host compatibility, not cross-platform floating-point admission.",
    }
    (output / "source-report.json").write_text(json.dumps(report, indent=2) + "\n")
    (output / "catalog/source.json").write_text(
        json.dumps(
            {
                "version": "canonical-spruce-inputs-1",
                "recipe": physical,
                "canonical_seed": settings["seed"],
                "input_sha256": {str(path): report["input_sha256"]},
                "implementation_sha256": report["implementation_sha256"],
                "template_subpixel_samples": settings["subpixel_samples"],
            },
            indent=2,
        )
        + "\n"
    )
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=INPUT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--verify-reference", action="store_true")
    args = parser.parse_args()
    report = create_source(
        args.input, args.output, verify_reference=args.verify_reference
    )
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "graph_identity",
                    "plant_seed",
                    "catalog_seed",
                    "reference_fields_verified",
                    "build_s",
                )
            }
        ),
        flush=True,
    )
