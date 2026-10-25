"""Frozen building parent: current-policy source model, gray control and materials."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_appearance import policy_digest
from building_mass_construction_materials import construction
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from compare_directed_groups import retained_subset, transformed
from compare_group_coverage import front_source
from compare_vertex_cluster_meshes import setup_view
from full_view_reference import load
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from triangle_coverage_reference import integrate
from urban_parent_light import display


def read(path):
    with np.load(path, allow_pickle=False) as archive:
        return dict(archive)


def candidate_mesh(original, product):
    if not np.array_equal(
        np.sort(np.r_[product["group_sources"], product["needs_sources"]]),
        np.unique(original["binding"][:, 1]),
    ):
        raise ValueError("candidate must retain every original source")
    rest = retained_subset(original, product["needs_sources"])
    return {
        "positions": np.vstack(
            (product["positions"], rest["positions"].astype(np.float32))
        ),
        "indices": np.vstack(
            (product["indices"], rest["indices"] + len(product["positions"]))
        ),
    }


def errors(image, reference):
    delta = abs(image["C"] + 0.1 * image["T"] - reference["C"] - 0.1 * reference["T"])
    return {
        "linear_rgb_l1_channel_sum": float(delta.sum()),
        "linear_rgb_maximum_channel_error": float(delta.max()),
        "premultiplied_C_l1_channel_sum": float(abs(image["C"] - reference["C"]).sum()),
        "T_l1_px2": float(abs(image["T"] - reference["T"]).sum()),
    }


def pictures(images, view, box, output):
    width, height = map(int, box[1] - box[0])
    panel = Image.new(
        "RGB", (4 * (width * 8 + 16), 2 * (height * 8 + 36)), (24, 24, 24)
    )
    draw = ImageDraw.Draw(panel)
    for i, (name, fields) in enumerate(images.items()):
        rgb = display(fields["C"] + 0.1 * fields["T"])
        full = np.full((view[4], view[3], 3), display(np.array([0.1] * 3)), np.uint8)
        full[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = rgb
        Image.fromarray(full).save(output / (name + ".png"))
        x, y = i * (width * 8 + 16) + 8, height * 8 + 64
        panel.paste(
            Image.fromarray(rgb).resize(
                (width * 8, height * 8), Image.Resampling.NEAREST
            ),
            (x, 28),
        )
        draw.text((x, 6), name, fill="white")
        reference = display(images["original"]["C"] + 0.1 * images["original"]["T"])
        delta = np.clip(
            abs(rgb.astype(int) - reference.astype(int)) * 4, 0, 255
        ).astype(np.uint8)
        panel.paste(
            Image.fromarray(delta).resize(
                (width * 8, height * 8), Image.Resampling.NEAREST
            ),
            (x, y),
        )
        draw.text((x, y - 22), "4x display error vs original", fill="white")
    panel.save(output / "comparison.png")


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    at, appearance, product = policy(), read(args.appearance), read(args.geometry)
    if (
        appearance["policy_sha256"].tobytes() != policy_digest(at)
        or appearance["geometry_sha256"].tobytes()
        != hashlib.sha256(args.geometry.read_bytes()).digest()
    ):
        raise ValueError(
            "appearance must match the current policy and exact frozen geometry"
        )
    wall, cpu = time.perf_counter(), time.process_time()
    native, document, context, receipt = load(args.input)
    original = transformed(native, product["basis"], product["anchor"])
    candidate = candidate_mesh(original, product)
    frame = product | {
        "source_bounds": np.array(
            [original["positions"].min(0), original["positions"].max(0)]
        )
    }
    view, _, box, _, _ = setup_view(
        {"positions": np.vstack((original["positions"], candidate["positions"]))},
        frame,
        document,
        context,
        {"direction": [0.45, -0.5, 0.7]},
    )
    fields, needs = face_materials(original, at)
    table_path = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    tables = read(table_path)
    images, reports = {}, {}
    for name, mesh in (
        ("original", original),
        ("neutral", candidate),
        ("material", candidate),
    ):
        images[name], reports[name] = evaluate(
            mesh,
            original,
            fields,
            appearance,
            product,
            view,
            box,
            tables,
            parent=name != "original",
            neutral=name == "neutral",
        )
        reports[name].update(errors(images[name], images["original"]))
        if name != "neutral":
            oriented, _ = front_source(mesh, view[0])
            area, _, _ = integrate(oriented, view, box)
            error = abs((1 - images[name]["T"][..., 0]) - area)
            reports[name]["quadrature_coverage_l1_vs_continuous_px2"] = float(
                error.sum()
            )
            reports[name]["quadrature_coverage_maximum_error"] = float(error.max())
    images["half_transition"] = {
        k: (images["original"][k] + images["material"][k]) / 2 for k in ("C", "T")
    }
    construction_work = construction(
        original, candidate, fields, appearance, product, tables, args.output
    )
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "receipt": receipt,
        "appearance_sha256": hashlib.sha256(args.appearance.read_bytes()).hexdigest(),
        "energy_tables_sha256": hashlib.sha256(table_path.read_bytes()).hexdigest(),
        "roi": box.tolist(),
        "resolution": [view[3], view[4]],
        "view": "moving-023",
        "original_material_needs": needs,
        "construction_work": construction_work,
        "reports": reports,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "comparison_process_peak_bytes": peak_rss_bytes(),
        },
        "scope": "One finite 16x16 point quadrature per pixel, not an exact PBR integral. Current owned-policy steady-repeat opaque material model on actual original UV/colour fields; captured native material binding and stored shading normals are not qualified. No shadows, IBL, finite pattern phases, overhang/recessed/base equivalence, emission, night, native/device/world/p99 admission. Geometric normals remain unaveraged. Gray control changes only grouped parent surfaces, retains original-source remainder. Half transition is a volatile premultiplied C/T endpoint blend at the same camera, not a world/depth/caster/LOD admission.",
    }
    np.savez(
        args.output / "images.npz",
        **{
            name + "__" + k: v
            for name, image in images.items()
            for k, v in image.items()
        },
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    pictures(images, view, box, args.output)
    print(
        json.dumps(
            {
                "reports": reports,
                "original_material_needs": len(needs),
                "cost": report["cost"],
            }
        ),
        flush=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "geometry", "appearance", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
