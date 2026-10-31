"""Construct one recorded view's requested source corners from cached domains and recipes.

Research reference only; no native World, device, visibility or fallback admission.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from prepared_surface_recipes import decode
from source_domain_buffer import restore_source
from source_domain_chunk_study import decode_raw
from source_patch_product import FIELDS
from welded_source_patch import source_corners


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(args):
    view = json.loads(args.view.read_text())
    bounds = args.bounds.read_bytes()
    if digest(bounds) != view["bounds_sha256"]:
        raise ValueError("Recorded selection must bind the same source bounds")
    with np.load(io.BytesIO(bounds), allow_pickle=False) as z:
        binding_sha = z["source_binding"].tobytes().hex()
        index_sha = z["index_digest"].tobytes().hex()
        hashes = {
            (int(o), int(n)): h.tobytes()
            for o, n, h in zip(
                z["group_offsets"], z["group_lengths"], z["group_digests"], strict=True
            )
        }
    binding_raw = args.binding.read_bytes()
    if digest(binding_raw) != binding_sha:
        raise ValueError("Recorded native source frame required")
    capture = json.loads(binding_raw)["capture_frame"]
    basis = np.array([capture["east"], capture["up"], -np.array(capture["north"])])
    row = next(row for row in view["cases"] if row["angle_degrees"] == args.angle)
    if row["selected_original_fallback_sources"]:
        raise ValueError(
            "Original fallback requires native consumer; no replacement fabricated"
        )
    selected = set(row["selected_source_ids"])
    recipe_index = (args.recipes / "index.json").read_bytes()
    if digest(recipe_index) != args.recipes_index_sha256:
        raise ValueError("Exact prepared recipe index required")
    metadata = json.loads(recipe_index)
    recipes, recipe_reads, recipe_bytes, decoded_cells = {}, 0, len(recipe_index), 0
    for cell in metadata:
        ordinal_raw = (args.recipes / cell["source_file"]).read_bytes()
        recipe_bytes += len(ordinal_raw)
        if digest(ordinal_raw) != cell["source_sha256"]:
            raise ValueError("Prepared source membership changed")
        ordinals = np.load(io.BytesIO(ordinal_raw), allow_pickle=False)
        if not selected.intersection(map(int, ordinals)):
            continue
        raw = (args.recipes / cell["file"]).read_bytes()
        recipe_reads += 1
        recipe_bytes += len(raw)
        if digest(raw) != cell["sha256"] or len(raw) != cell["bytes"]:
            raise ValueError("Prepared recipe cell changed")
        surfaces, _ = decode(raw)
        decoded_cells += len(surfaces)
        if len(ordinals) != len(surfaces):
            raise ValueError("Complete source/recipe association required")
        recipes.update(
            (int(i), surface)
            for i, surface in zip(ordinals, surfaces, strict=True)
            if int(i) in selected
        )
    if set(recipes) != selected or any(surface is None for surface in recipes.values()):
        raise ValueError("Every selected source requires its prepared recipe")
    raw_index = (args.chunk / "index.npy").read_bytes()
    if digest(raw_index) != index_sha:
        raise ValueError("Prepared source index changed")
    index = np.load(io.BytesIO(raw_index), allow_pickle=False)
    records = {int(r["source"]): (int(r["offset"]), int(r["bytes"])) for r in index}
    if not selected.issubset(records):
        raise ValueError("Unindexed source requires whole original")
    chunks = {records[i] for i in selected}
    result, roles, identities, group_bytes = [], [], [], 0
    with (args.chunk / "groups.bin").open("rb") as stream:
        for offset, length in sorted(chunks):
            stream.seek(offset)
            raw = stream.read(length)
            if len(raw) != length or hashlib.sha256(raw).digest() != hashes.get(
                (offset, length)
            ):
                raise ValueError("Incomplete or modified group requires original")
            group_bytes += len(raw)
            group = decode_raw(raw)
            for identity in set(map(int, group["source_ids"])) & selected:
                patches = restore_source(group, identity, recipes[identity])
                corners, _ = source_corners(patches, basis)
                result.append(np.concatenate([corners[k] for k in FIELDS], axis=-1))
                roles.append(corners["material"])
                identities.extend([identity] * len(corners["material"]))
    if set(identities) != selected:
        raise ValueError("Every selected source needs its whole constructed candidate")
    np.savez(
        args.output,
        corners=np.concatenate(result),
        material=np.concatenate(roles),
        source=np.asarray(identities, dtype="<i4"),
    )
    report = {
        "angle": args.angle,
        "selected_sources": len(selected),
        "constructed_triangles": len(identities),
        "selected_recipe_cells": recipe_reads,
        "decoded_recipe_cell_sources": decoded_cells,
        "recipe_index_memberships_and_cells_read_bytes": recipe_bytes,
        "group_bytes_reread": group_bytes,
        "index_bytes_reread": len(raw_index),
        "output_bytes": args.output.stat().st_size,
        "scope": "Recorded finite selected candidates only, constructed from already generated domains and prepared material recipes. Recipes are decoded by whole source cell; extra selected-view metadata and source-frame reads are excluded from this partial byte count. No native raster, World, GPU, device time or original fallback admission. This stage repeats the earlier group reads in a separate process.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("view", "bounds", "binding", "recipes", "chunk", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--recipes-index-sha256", required=True)
    parser.add_argument("--angle", type=int, default=65)
    run(parser.parse_args())
