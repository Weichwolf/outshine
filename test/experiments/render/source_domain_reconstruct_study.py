"""Construct one recorded view's requested source corners from cached domains and recipes.

Research reference only; no native World, device, visibility or raster admission.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA
from prepared_surface_recipes import decode
from source_domain_buffer import DomainBudget, restore_source
from source_domain_chunk_study import decode_raw
from source_domain_prepared_view import select_sources
from source_original_ranges import read_ranges
from source_patch_product import FIELDS
from source_recipe_ranges import load_ranges
from welded_source_patch import TriangleBudget, source_corners


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked_selection(row, expected):
    requested = set(row["selected_source_ids"])
    if (
        len(requested) != len(row["selected_source_ids"])
        or row["selected_sources"] != len(requested)
        or requested != set(expected)
    ):
        raise ValueError("Incomplete or changed source selection; retain originals")
    return requested


def original_fallback(directory, binding_sha, fallback_ids):
    """Read verified whole originals and preserve their actual native material rule."""
    range_index = (directory / "index.npy").read_bytes()
    metadata = json.loads((directory / "report.json").read_text())
    if (
        metadata["source_binding_sha256"] != binding_sha
        or digest(range_index) != metadata["index_sha256"]
        or metadata["encoding"] != "source_indexed"
    ):
        raise ValueError("Same bound original source ranges required")
    rows, work = read_ranges(
        directory,
        np.load(io.BytesIO(range_index), allow_pickle=False),
        sorted(fallback_ids),
        indexed=True,
    )
    labels = rows["binding"]
    if set(map(int, labels[:, 1])) != fallback_ids:
        raise ValueError("Whole original source range missing")
    materials = labels[:, 4].copy()
    metres = np.any(rows["corners"][:, :, 8:10] != 0, axis=(1, 2))
    materials[(materials == 0) & metres] = 2
    if np.any((materials < 0) | (materials > 2)):
        raise ValueError("Unknown native material stream")
    return rows, materials, work, len(range_index) + work["read_bytes"]


def run(args, *, retained_groups=None, selected_sources=None):
    max_controls = getattr(args, "max_controls", None)
    max_triangles = getattr(args, "max_triangles", None)
    original_ranges = getattr(args, "original_ranges", None)
    if (
        max_controls is not None or max_triangles is not None
    ) and original_ranges is None:
        raise ValueError("Bounded candidates require complete original ranges")
    view = json.loads(args.view.read_text())
    bounds = args.bounds.read_bytes()
    if digest(bounds) != view["bounds_sha256"]:
        raise ValueError("Recorded selection must bind the same source bounds")
    with np.load(io.BytesIO(bounds), allow_pickle=False) as z:
        prepared = {key: z[key] for key in ("source_ids", "bounds", "normal")}
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
    if selected_sources is None:
        with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as z:
            views, _ = read_controls(z["near-direction/view_words"])
            expected = select_sources(
                prepared, views[args.angle], z["placements/words"]
            )
    else:
        expected = selected_sources
    requested = checked_selection(row, expected)
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
    if args.span_index and selected:
        span_raw = args.span_index.read_bytes()
        recipe_bytes += len(span_raw)
        if digest(span_raw) != args.span_index_sha256:
            raise ValueError("Exact prepared span index required")
        with np.load(io.BytesIO(span_raw), allow_pickle=False) as spans:
            recipes, work = load_ranges(
                spans["table"],
                spans["sha256"],
                [(cell["cell"], args.recipes / cell["file"]) for cell in metadata],
                selected,
            )
        recipe_reads = work["recipe_range_reads"]
        recipe_bytes += work["recipe_range_bytes"]
        decoded_cells = work["recipe_sources_decoded"]
    elif selected:
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
    result, roles, identities, group_bytes, reused_bytes = [], [], [], 0, 0
    budget_fallbacks = {}
    with (args.chunk / "groups.bin").open("rb") as stream:
        for offset, length in sorted(chunks):
            if retained_groups is None:
                stream.seek(offset)
                raw = stream.read(length)
                group_bytes += len(raw)
            else:
                raw = retained_groups.get((offset, length), b"")
                reused_bytes += len(raw)
            if len(raw) != length or hashlib.sha256(raw).digest() != hashes.get(
                (offset, length)
            ):
                raise ValueError("Incomplete or modified group requires original")
            group = decode_raw(raw)
            for identity in set(map(int, group["source_ids"])) & selected:
                try:
                    patches = restore_source(
                        group, identity, recipes[identity], max_controls=max_controls
                    )
                    corners, _ = source_corners(
                        patches, basis, max_triangles=max_triangles
                    )
                except (DomainBudget, TriangleBudget) as error:
                    budget_fallbacks[identity] = str(error)
                    continue
                result.append(np.concatenate([corners[k] for k in FIELDS], axis=-1))
                roles.append(corners["material"])
                identities.extend([identity] * len(corners["material"]))
    if set(identities) != selected - budget_fallbacks.keys():
        raise ValueError("Every selected source needs its whole constructed candidate")
    constructed_triangles = len(identities)
    fallback_binding = np.empty((0, 6), dtype="<i4")
    fallback_bytes = 0
    fallback_ids.update(budget_fallbacks)
    range_work = {"read_calls": 0, "read_bytes": 0}
    if fallback_ids and original_ranges is not None:
        rows, materials, range_work, fallback_bytes = original_fallback(
            original_ranges, binding_sha, fallback_ids
        )
        fallback_binding = rows["binding"].copy()
        result.append(rows["corners"])
        roles.append(materials)
        identities.extend(map(int, fallback_binding[:, 1]))
    elif fallback_ids:
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
        "max_controls": max_controls,
        "max_triangles": max_triangles,
        "budget_fallbacks": budget_fallbacks,
        "original_range_work": range_work,
        "selected_sources": len(requested),
        "selection_check": "recomputed" if selected_sources is None else "in_process",
        "constructed_triangles": constructed_triangles,
        "fallback_sources": len(fallback_ids),
        "fallback_triangles": len(fallback_binding),
        "fallback_bytes_reread": fallback_bytes,
        "recipe_read_calls": recipe_reads,
        "recipe_sources_decoded": decoded_cells,
        "recipe_metadata_and_payload_bytes": recipe_bytes,
        "recipe_read_mode": "span" if args.span_index else "whole_cell",
        "group_bytes_reread": group_bytes,
        "group_bytes_reused": reused_bytes,
        "retained_group_blob_bytes": sum(map(len, retained_groups.values()))
        if retained_groups is not None
        else 0,
        "index_bytes_reread": len(raw_index),
        "output_bytes": args.output.stat().st_size,
        "scope": "Recorded finite selected candidates from already generated domains and prepared material recipes, plus unchanged whole original fallbacks. Extra selected-view metadata and source-frame reads are excluded from this partial byte count. No native raster, World, GPU or device admission. Group bytes are either reread or passed in-process from selection; other metadata and any required fallback are still reread.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("view", "bounds", "binding", "recipes", "chunk", "fallback", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--recipes-index-sha256", required=True)
    parser.add_argument("--span-index", type=Path)
    parser.add_argument("--span-index-sha256")
    parser.add_argument("--max-controls", type=int)
    parser.add_argument("--max-triangles", type=int)
    parser.add_argument("--original-ranges", type=Path)
    parser.add_argument("--angle", type=int, default=65)
    args = parser.parse_args()
    if (args.span_index is None) != (args.span_index_sha256 is None):
        parser.error("--span-index and --span-index-sha256 must be supplied together")
    run(args)
