"""Cache actual physical grass/herb forms and paired whole rooted populations."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import GroundCoverPlan
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh
from scipy.spatial import cKDTree
from understory_mixture import prepare, selection, shuffled_control
from understory_plan import LilyStudy


def grass_model(seed, scale):
    mesh = graminoid_tuft_mesh(seed, segments=3)
    mesh["vertices"] *= scale
    normals = mesh["normals"] / scale
    mesh["normals"] = normals / np.linalg.norm(normals, axis=1, keepdims=True)
    mesh["uv"] = mesh["uv"][mesh["faces"].ravel()]
    mesh["surface"] = np.full(len(mesh["faces"]), 1 + seed % 3, np.uint8)
    return mesh


def models(output, names):
    records = []
    for name in names:
        for variant in range(8):
            began = time.perf_counter()
            recipe = None
            if name == "convallaria-study":
                mesh, _, recipe = LilyStudy(121 + variant).mesh(
                    leaf_segments=8, bell_segments=3, bell_sides=18
                )
            else:
                scale = np.array(
                    (1.15, 1.15, 0.6) if name == "short-grass" else (0.85, 0.85, 1.5)
                )
                mesh = grass_model(17 + variant, scale)
            elapsed = time.perf_counter() - began
            label = f"{name}-{variant}"
            np.savez_compressed(output / (label + ".npz"), **mesh)
            if recipe:
                (output / (label + "-plan.json")).write_text(
                    json.dumps(recipe, indent=2) + "\n"
                )
            records.append(
                {
                    "form": name,
                    "variant": variant,
                    "file": label + ".npz",
                    "vertices": len(mesh["vertices"]),
                    "triangles": len(mesh["faces"]),
                    "payload_bytes": sum(value.nbytes for value in mesh.values()),
                    "mesh_s": elapsed,
                }
            )
    mesh, _, recipe = LilyStudy(121).mesh(
        leaf_segments=24, bell_segments=12, bell_sides=48
    )
    np.savez_compressed(output / "convallaria-near.npz", **mesh)
    (output / "convallaria-near-plan.json").write_text(
        json.dumps(recipe, indent=2) + "\n"
    )
    return records


def neighbourhood(poses):
    distance, indices = cKDTree(poses["origins"][:, :2]).query(
        poses["origins"][:, :2], k=2
    )
    nearest = indices[:, 1]
    return {
        "nearest_median_m": float(np.median(distance[:, 1])),
        "same_form_neighbour_fraction": float(
            np.mean(poses["classes"] == poses["classes"][nearest])
        ),
    }


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    plan = GroundCoverPlan(113, 150)
    sites = plan.sites((-4, -4), (4, 4))
    mixture, fields = prepare()
    timings, products = {}, {}
    for method in ("stream", "matrix"):
        began = time.perf_counter()
        products[method] = selection(sites, mixture, fields, score_mode=method)
        timings[method] = time.perf_counter() - began
    for key in products["stream"]:
        np.testing.assert_array_equal(products["stream"][key], products["matrix"][key])
    grouped, control = products["stream"], shuffled_control(products["stream"])
    for label, poses in (("grouped", grouped), ("shuffled", control)):
        np.savez_compressed(output / (label + ".npz"), **poses)
    np.savez_compressed(output / "roots.npz", **sites)
    saved = {
        "root_plan": plan.recipe(),
        "mixture": mixture.recipe(),
        "fields": fields.recipe(),
        "strength": 0.92,
        "names": mixture.taxa,
        "scope": "Authored short/tall generic grass forms and Convallaria study; not a habitat/species admission or quota model.",
    }
    (output / "stand-plan.json").write_text(json.dumps(saved, indent=2) + "\n")
    records = models(output, mixture.taxa)
    triangles = sum(
        np.sum(
            (grouped["classes"] == mixture.taxa.index(row["form"]))
            & (grouped["variants"] == row["variant"])
        )
        * row["triangles"]
        for row in records
    )
    summary = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "roots": len(sites["ids"]),
        "timings_s": timings,
        "names": mixture.taxa,
        "class_counts": np.bincount(
            grouped["classes"], minlength=len(mixture.taxa)
        ).tolist(),
        "grouped": neighbourhood(grouped),
        "shuffled": neighbourhood(control),
        "instanced_triangles": int(triangles),
        "population_bytes": sum(value.nbytes for value in grouped.values()),
        "models": records,
    }
    (output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(
        json.dumps({key: value for key, value in summary.items() if key != "models"}),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
