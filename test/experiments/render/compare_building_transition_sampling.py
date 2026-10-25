"""Physical root crossing: whole C/T blend versus complementary pre-shading ownership."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_motion_reference import (
    motion_bounds,
    residual_change,
    residuals,
    trajectory,
)
from building_source_tree import select_cut, sources_at
from building_transition import independent_fade_over, mix_layers
from building_transition_sampling import owned_masks, ownership_ranks, sum_owned
from building_tree_load import read_tree
from building_tree_queries import view
from building_tree_reference import reference
from building_tree_transfer import transfers
from compare_building_hierarchy import complete, write_images
from compare_building_matte_motion import LIGHTS
from compare_building_source_models import contact
from compare_building_transition import endpoints, original_union_cost
from compare_mass_appearance import errors, read
from process_memory import peak_rss_bytes


def render_frame(scene, camera, box, weight, ranks, tables, lighting):
    original, fields = scene["original"], scene["fields"]
    products, materials, meshes = (
        scene[k] for k in ("products", "materials", "meshes")
    )
    images, work = {}, {}
    images["original"], work["original"] = evaluate(
        original,
        original,
        fields,
        materials["finer"],
        products["finer"],
        camera,
        box,
        tables,
        **lighting,
    )

    def endpoint(name, mask=None):
        return transfers(
            meshes[name],
            original,
            fields,
            products[name],
            materials[name],
            camera,
            box,
            tables,
            16,
            sample_mask=mask,
            **lighting,
        )

    for name in ("finer", "coarser"):
        images[name], work[name] = endpoint(name)
    images["ideal_mix"] = mix_layers(images["finer"], images["coarser"], weight)
    images["independent_over"] = independent_fade_over(
        images["finer"], images["coarser"], weight
    )
    actual = {}
    for granularity, (rank, stratum_size) in ranks.items():
        finer_mask, coarser_mask, actual[granularity] = owned_masks(
            rank, stratum_size, weight
        )
        parts = []
        for name, mask in (("finer", finer_mask), ("coarser", coarser_mask)):
            if mask.any():
                piece, cost = endpoint(name, mask)
            else:
                piece = {k: np.zeros_like(v) for k, v in images[name].items()}
                cost = {"ggx_lobe_evaluations": 0, "raster_work_summed_over_groups": {}}
            parts.append(piece)
            work[granularity + "_owned_" + name] = cost
        images[granularity + "_owned"] = sum_owned(*parts)
    return images, work, actual


def prepared(args, cell):
    root = args.input / f"cell-{cell}"
    case = json.loads((root / "report.json").read_text())
    full_tree, tree = read_tree(root), read_tree(root, lineage=False)
    queries = case["queries"][args.azimuth * 2 : args.azimuth * 2 + 2]
    if len(queries) != 2:
        raise ValueError("existing root pair required")
    for query in queries:
        covered = np.r_[
            full_tree["unbounded_sources"],
            np.concatenate([sources_at(full_tree, node) for node in query["nodes"]]),
        ]
        if not np.array_equal(np.sort(covered), full_tree["selected_sources"]):
            raise ValueError("each Ready cut must cover the complete same sources")
    products, materials, loading, resident = endpoints(
        root, case, tree, queries, policy()
    )
    original, provenance = reference(
        args.source,
        cell,
        full_tree,
        native_input=args.native_input,
        native_cell=args.native_cell,
    )
    fields, needs = face_materials(original, policy())
    if needs:
        raise ValueError("complete qualified fine-reference materials required")
    meshes, storage = {}, {}
    for name, product in products.items():
        meshes[name], storage[name] = complete(original, product)
    camera = view(queries[0] | {"eye": queries[0]["render_eye"]})
    return (
        {
            "original": original,
            "fields": fields,
            "products": products,
            "materials": materials,
            "meshes": meshes,
        },
        camera,
        tree,
        {
            "reference": provenance,
            "loading": loading,
            "both_ready_parent_array_bytes": resident,
            "storage": storage,
            "original_union": original_union_cost(original, products),
            "both_ready_mesh_assembly_array_bytes": sum(
                v.nbytes for mesh in meshes.values() for v in mesh.values()
            ),
            "root_transition_distance_m": queries[0]["render_distance_m"],
            "finer_ready_nodes": queries[0]["nodes"],
            "coarser_ready_nodes": queries[1]["nodes"],
        },
    )


def run_light(output, scene, cameras, times, box, ranks, tree, tables, lighting):
    output.mkdir()
    reports, panels, previous = [], [], None
    for i, camera in enumerate(cameras):
        weight = i / (len(cameras) - 1)
        images, work, actual = render_frame(
            scene, camera, box, weight, ranks, tables, lighting
        )
        cut, selection = select_cut(tree, camera[0], camera[2])
        images["hard_cut"] = (
            images["coarser"] if cut.tolist() == [0] else images["finer"]
        )
        current = residuals(images)
        upper_t = np.maximum(images["finer"]["T"], images["coarser"]["T"])
        shared_opaque = (images["finer"]["T"] == 0) & (images["coarser"]["T"] == 0)
        report = {
            "seconds_from_midpoint": float(times[i]),
            "eye": camera[0].tolist(),
            "requested_coarser_weight": weight,
            "represented_coarser_weight": actual,
            "actual_hard_cut_nodes": cut.tolist(),
            "selection": selection,
            "errors_vs_original": {
                k: errors(v, images["original"])
                for k, v in images.items()
                if k != "original"
            },
            "owned_vs_ideal": {
                granularity: errors(images[granularity + "_owned"], images["ideal_mix"])
                for granularity in ranks
            },
            "work": work,
            "shared_opaque_pixels": int(shared_opaque.sum()),
            "T_on_shared_opaque_pixels": {
                name: float(images[name]["T"][shared_opaque].max())
                if shared_opaque.any()
                else None
                for name in (
                    "ideal_mix",
                    "pixel_owned",
                    "sample_owned",
                    "independent_over",
                )
            },
            "extra_T_beyond_endpoint_max_sum_px2": {
                name: float(np.maximum(images[name]["T"] - upper_t, 0).sum())
                for name in (
                    "ideal_mix",
                    "pixel_owned",
                    "sample_owned",
                    "independent_over",
                )
            },
            "residual_change": residual_change(
                previous, current, times[i] - times[i - 1]
            )
            if previous is not None
            else None,
        }
        keyframe = i in (0, len(cameras) // 2, len(cameras) - 1)
        report["retained_counterexample"] = any(
            report["errors_vs_original"][granularity + "_owned"][k]
            > report["errors_vs_original"]["ideal_mix"][k]
            for granularity in ranks
            for k in ("linear_rgb_l1_channel_sum", "T_l1_px2")
        )
        if keyframe or report["retained_counterexample"]:
            write_images(output, f"frame-{i}", images, {}, camera, box)
        if keyframe:
            panels.append(
                (
                    {
                        k: images[k]
                        for k in (
                            "original",
                            "ideal_mix",
                            "pixel_owned",
                            "sample_owned",
                            "independent_over",
                        )
                    },
                    box,
                )
            )
        reports.append(report)
        previous = current
    contact(panels, output)
    return {"lighting": lighting, "frames": reports}


def run_case(args, cell, tables):
    scene, camera, tree, report = prepared(args, cell)
    times, cameras = trajectory(camera, [0, 0, -args.speed], args.duration, args.frames)
    cuts = [select_cut(tree, c[0], c[2])[0].tolist() for c in cameras]
    if cuts[0] == [0] or cuts[-1] != [0]:
        raise ValueError("declared physical path must cross from children to root")
    if any(
        cut not in (report["finer_ready_nodes"], report["coarser_ready_nodes"])
        for cut in cuts
    ):
        raise ValueError("every physical cut must match one of the two Ready products")
    positions = np.vstack(
        [
            scene["original"]["positions"],
            *(mesh["positions"] for mesh in scene["meshes"].values()),
        ]
    )
    box = motion_bounds(positions, cameras)
    width, height = map(int, box[1] - box[0])
    cpu = time.process_time()
    ranks = {
        granularity: ownership_ranks(width, height, granularity=granularity)
        for granularity in ("pixel", "sample")
    }
    output = args.output / f"cell-{cell}"
    output.mkdir()
    report.update(
        {
            "cell": cell,
            "speed_m_s": args.speed,
            "duration_s": args.duration,
            "fixed_screen_roi": box.tolist(),
            "ownership_models": {
                k: {"rank_array_bytes": v[0].nbytes, "stratum_size": v[1]}
                for k, v in ranks.items()
            },
            "sample_rank_cpu_ms": (time.process_time() - cpu) * 1000,
            "two_owned_mask_bytes_per_model": 2 * len(ranks["pixel"][0]),
            "lights": {},
        }
    )
    for name, lighting in LIGHTS.items():
        report["lights"][name] = run_light(
            output / name, scene, cameras, times, box, ranks, tree, tables, lighting
        )
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({"completed_cell": cell, "completed_light": name}), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--native-cell", type=int)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--azimuth", type=int, choices=(0, 1), default=0)
    parser.add_argument("--speed", type=float, default=200)
    parser.add_argument("--duration", type=float, default=1)
    parser.add_argument("--frames", type=int, default=9)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    tables = read(
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": [run_case(args, cell, tables) for cell in args.cells],
    }
    report["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "peak_bytes": peak_rss_bytes(),
    }
    report["scope"] = (
        "Whole-cut pixel/sample ownership before material/GGX, same physical root crossing and full-source Ready cuts. Stratified fixed PCG64 seed0 at SS16; no seed/rate sweep or native pattern contract. Finite realized samples are not the ensemble mean. Sample-level CPU GGX counts do not prove savings with native pixel-rate shading; pixel ownership is separately measured. Both endpoint raster/Ready/remainder costs paid; no GPU/native/world/caster/night/temporal admission."
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["cost"]))


if __name__ == "__main__":
    main()
