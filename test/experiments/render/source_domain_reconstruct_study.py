"""Construct one recorded view's requested source corners from cached domains and recipes.

Research reference only; no native World, device, visibility or raster admission.
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
        fallback_sha = z["fallback_digest"].tobytes().hex()
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
    requested = set(row["selected_source_ids"])
    raw_index = (args.chunk / "index.npy").read_bytes()
    if digest(raw_index) != index_sha:
        raise ValueError("Prepared source index changed")
    index = np.load(io.BytesIO(raw_index), allow_pickle=False)
    records = {int(r["source"]): (int(r["offset"]), int(r["bytes"])) for r in index}
    selected = requested & records.keys()
    fallback_ids = requested - selected
    if len(fallback_ids) != row["selected_original_fallback_sources"]:
        raise ValueError("Complete recorded original fallback demand required")
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
    constructed_triangles = len(identities)
    fallback_binding = np.empty((0, 6), dtype="<i4")
    fallback_bytes = 0
    if fallback_ids:
        fallback_raw = args.fallback.read_bytes()
        if digest(fallback_raw) != fallback_sha:
            raise ValueError("Original fallback identity changed")
        with np.load(io.BytesIO(fallback_raw), allow_pickle=False) as z:
            labels = z["actual_triangle_binding"]
            include = np.isin(labels[:, 1], list(fallback_ids))
            fallback_binding = labels[include].copy()
            if set(map(int, fallback_binding[:, 1])) != fallback_ids:
                raise ValueError("Every requested original fallback must remain whole")
            result.append(z["corners"][include].copy())
            roles.append(z["material"][include].copy())
            identities.extend(map(int, fallback_binding[:, 1]))
        fallback_bytes = len(fallback_raw)
    if set(identities) != requested:
        raise ValueError("Every selected source must have complete output")
    np.savez(
        args.output,
        corners=np.concatenate(result) if result else np.empty((0, 3, 14), "<f4"),
        material=np.concatenate(roles) if roles else np.empty(0, "<i4"),
        source=np.asarray(identities, dtype="<i4"),
        fallback_binding=fallback_binding,
    )
    report = {
        "angle": args.angle,
        "selected_sources": len(requested),
        "constructed_triangles": constructed_triangles,
        "fallback_sources": len(fallback_ids),
        "fallback_triangles": len(fallback_binding),
        "fallback_bytes_reread": fallback_bytes,
        "selected_recipe_cells": recipe_reads,
        "decoded_recipe_cell_sources": decoded_cells,
        "recipe_index_memberships_and_cells_read_bytes": recipe_bytes,
        "group_bytes_reread": group_bytes,
        "index_bytes_reread": len(raw_index),
        "output_bytes": args.output.stat().st_size,
        "scope": "Recorded finite selected candidates from already generated domains and prepared material recipes, plus unchanged whole original fallbacks. Recipes are decoded by whole source cell; extra selected-view metadata and source-frame reads are excluded from this partial byte count. No native raster, World, GPU or device admission. This stage repeats earlier group/fallback reads in a separate process.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("view", "bounds", "binding", "recipes", "chunk", "fallback", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--recipes-index-sha256", required=True)
    parser.add_argument("--angle", type=int, default=65)
    run(parser.parse_args())
