"""Compare single triangles and certified joined roofs on the same native seeds."""

import argparse
import hashlib
import json
import tempfile
import time
from collections import Counter
from pathlib import Path

import numpy as np
from building_positions import RECIPES
from building_roof_domains import planar_roof_domains, triangulate_domain
from building_roof_part_selection import contour_selection, parts
from building_roof_positions import roof_positions
from building_roof_selection import emit_roof, images
from outshine_ref.geometry.occlusion import convex_patch_umbra
from outshine_ref.geometry.occlusion_hierarchy import BoundsHierarchy
from roof_fusion import fused_roof_umbra
from roof_part_comparison import part_comparison


def seed_groups(source, *, vertex_recipe="float64"):
    surface, groups = source["surface"], []
    if surface is None:
        return groups
    for shape in surface["shapes"]:
        try:
            pieces = planar_roof_domains(shape)
            local = [triangulate_domain(piece) for piece in pieces]
        except ValueError:
            continue
        groups.append(
            [
                roof_positions(
                    triangles, piece["plane_enu"], surface, recipe=vertex_recipe
                )
                for piece, triangles in zip(pieces, local, strict=True)
                if len(triangles)
            ]
        )
    return groups


def fixed_seeds(sources, bounds, known, eye, vertex_recipe):
    """Source identities come only from metadata, identically for both recipes."""
    order = np.flatnonzero(known)
    distance = np.linalg.norm(bounds[order].mean(axis=1) - eye, axis=1)
    order = order[np.argsort(distance, kind="stable")][:32]
    geometry = {
        int(index): emit_roof(sources[index], vertex_recipe=vertex_recipe)
        for index in order
    }
    protected = np.zeros(len(sources), dtype=bool)
    protected[order] = True
    counts = {
        "retained_seed_sources": len(order),
        "source_indices": order.tolist(),
        "accepted_seed_roof_triangles": sum(
            len(value[0]) for value in geometry.values()
        ),
    }
    return geometry, protected, counts


def try_join(group, bounds, observer, source_index, counts, level):
    if not len(group) or (group < bounds[0]).any() or (group > bounds[1]).any():
        counts[f"{level}_outside_source_bound_or_empty"] += 1
        return None
    try:
        cone = fused_roof_umbra(group, observer, certified_opaque=True)
    except ValueError as error:
        counts[f"{level}_rejected: {error}"] += 1
        return None
    if not len(cone.low):
        counts[f"{level}_not_front_facing_for_entire_box"] += 1
        return None
    counts[f"{level}_certified_groups"] += 1
    counts[f"{level}_triangles_in_certified_groups"] += len(group)
    return cone, source_index


def packets(sources, geometry, bounds, observer, *, vertex_recipe="float64"):
    single, fused, joined, counts = [], [], [], Counter()
    for source_index, (emitted, _) in geometry.items():
        shapes = seed_groups(sources[source_index], vertex_recipe=vertex_recipe)
        groups = [group for shape in shapes for group in shape]
        combined = np.concatenate(groups) if groups else np.empty((0, 3, 3))
        if not np.array_equal(combined, emitted):
            raise ValueError("fusion geometry differs from retained accepted seed mesh")
        piece_packets = []
        for group in groups:
            original = []
            for triangle in group:
                if (triangle < bounds[source_index, 0]).any() or (
                    triangle > bounds[source_index, 1]
                ).any():
                    counts["uncertified_seed_outside_source_bound"] += 1
                    continue
                try:
                    cone = convex_patch_umbra(triangle, observer, certified_opaque=True)
                except ValueError as error:
                    counts[f"single_rejected: {error}"] += 1
                    continue
                if len(cone.low):
                    original.append((cone, source_index))
            single.extend(original)
            cone = try_join(
                group, bounds[source_index], observer, source_index, counts, "piece"
            )
            piece_packets.append([cone] if cone is not None else original)
            fused.extend(piece_packets[-1])
        shape_packets, first = [], 0
        for shape in shapes:
            fallback = [
                item
                for packet in piece_packets[first : first + len(shape)]
                for item in packet
            ]
            first += len(shape)
            cone = (
                try_join(
                    np.concatenate(shape),
                    bounds[source_index],
                    observer,
                    source_index,
                    counts,
                    "shape",
                )
                if len(shape) > 1
                else None
            )
            shape_packets.extend([cone] if cone is not None else fallback)
        cone = (
            try_join(
                emitted, bounds[source_index], observer, source_index, counts, "source"
            )
            if len(shapes) > 1
            else None
        )
        joined.extend([cone] if cone is not None else shape_packets)
    counts.update(
        single_certificates=len(single),
        mixed_fused_certificates=len(fused),
        joined_source_certificates=len(joined),
    )
    return single, fused, joined, counts


def run(path, output, *, vertex_recipe="float64"):
    native = json.loads(path.read_bytes())
    sources = native["sources"]
    bounds = np.array(
        [
            [source["bounds"]["min"], source["bounds"]["max"]]
            if source["bounds"] is not None
            else [[np.nan] * 3] * 2
            for source in sources
        ]
    )
    known = np.isfinite(bounds).all(axis=(1, 2))
    eye = np.asarray(native["view"]["eye_ecef"]) - native["anchor_ecef"]
    observer = np.array([eye - 2, eye + 2])
    geometry, protected_sources, seed_counts = fixed_seeds(
        sources, bounds, known, eye, vertex_recipe
    )
    start = time.perf_counter()
    single, fused, joined, compile_counts = packets(
        sources, geometry, bounds, observer, vertex_recipe=vertex_recipe
    )
    compile_seconds = time.perf_counter() - start
    modes = (("single", single), ("fused", fused), ("joined", joined))
    source_tree = BoundsHierarchy(bounds, known=known)
    source_results = {
        name: source_tree.select(packet, protected=protected_sources)
        for name, packet in modes
    }
    rows, omissions = parts(sources, vertex_recipe=vertex_recipe)
    target_bounds = np.array([row["bounds"] for row in rows])
    protected_parts = np.array([protected_sources[row["source_index"]] for row in rows])
    target_tree = BoundsHierarchy(target_bounds, known=np.ones(len(rows), dtype=bool))
    target_results = {
        name: target_tree.select(
            [(cone, None) for cone, _ in packet], protected=protected_parts
        )
        for name, packet in modes
    }
    contour_masks, contour_work = {}, {}
    for name, packet in modes:
        contour_masks[name], contour_work[name] = contour_selection(
            rows, packet, protected_parts
        )
    report = {
        "scope": "Same accepted roof seed geometry per recipe, exact simple projected boundary/covered inner kernel and rational height slab. Roof-only experiment, not universal native mesh/world/shadow/device acceptance. Part bounds/supports still need post-selection emission checks.",
        "input": str(path.resolve()),
        "vertex_recipe": vertex_recipe,
        "seed_policy": "32 nearest known source envelopes; source identities independent of position recipe",
        "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "source_records": len(sources),
        "parts": len(rows),
        "seed_counts": dict(seed_counts),
        "compile_counts": dict(compile_counts),
        "compile_seconds_single_plus_fused": compile_seconds,
        "source_results": {
            name: {
                "hidden": np.flatnonzero(result.hidden).tolist(),
                "work": result.work,
            }
            for name, result in source_results.items()
        },
        "part_results": {
            name: {"hidden_count": int(result.hidden.sum()), "work": result.work}
            for name, result in target_results.items()
        },
        "contour_results": {
            name: {"hidden_count": int(mask.sum()), "work": contour_work[name]}
            for name, mask in contour_masks.items()
        },
        "omissions": dict(omissions),
    }
    if any(result.hidden.any() for result in source_results.values()):
        masks = {"whole": np.zeros(len(sources), dtype=bool)}
        masks.update({name: result.hidden for name, result in source_results.items()})
        report["source_images"] = images(
            native, sources, masks, geometry, output, vertex_recipe=vertex_recipe
        )
    if any(mask.any() for mask in contour_masks.values()):
        report["part_emission"] = part_comparison(
            native,
            sources,
            rows,
            contour_masks,
            dict(modes),
            geometry,
            output / "parts",
            vertex_recipe,
        )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "source_hidden": {
                    name: len(row["hidden"])
                    for name, row in report["source_results"].items()
                },
                "part_hidden": {
                    name: row["hidden_count"]
                    for name, row in report["part_results"].items()
                },
                "contour_hidden": {
                    name: row["hidden_count"]
                    for name, row in report["contour_results"].items()
                },
                "compile_counts": report["compile_counts"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--vertex-recipe", choices=RECIPES, default="float64")
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    target.mkdir(parents=True)
    run(args.input, target, vertex_recipe=args.vertex_recipe)
