"""Compare direct domain material coordinates with bound triangle interpolation.

Finite CPU diagnostic only. The domains are unsnapped, not native raster/PBR.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from native_capture_meshes import fields_at
from PIL import Image, ImageDraw
from source_domain_buffer import restore_source
from source_domain_ray_reference import intersect
from source_domain_ray_study import source_recipe
from source_material_patches import corner_fields
from triangle_image_reference import (
    image_rays,
    intersect_source,
    low_eye_view,
    prepare_source,
    visible_pixel_bounds,
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def direct_fields(patches, origins, directions, depth, owners):
    uv = np.zeros((len(depth), 2), np.float32)
    metres = np.zeros_like(uv)
    qualified = np.zeros(len(depth), bool)
    for i, patch in enumerate(patches):
        selected = np.flatnonzero(owners == i)
        if not len(selected):
            continue
        s = patch.geometry
        axes = np.asarray(s.axes, float)
        points = (
            origins[selected] + depth[selected, None] * directions[selected] - s.origin
        )
        local = points @ axes.T @ np.linalg.inv(axes @ axes.T)
        try:
            values, metric, _ = corner_fields(patch, local)
        except ValueError:
            continue
        uv[selected], metres[selected], qualified[selected] = values, metric, True
    return {"uv": uv, "metres": metres, "qualified": qualified}


def flat_code(uv):
    value = np.asarray(uv[:, 0], np.float32)
    return np.where(value < 0, value, np.floor(value / np.float32(256)))


def compare(reference, fields, direct):
    common = (reference >= 0) & direct["qualified"]
    errors = {}
    for key in ("uv", "metres"):
        delta = np.abs(fields[key][common] - direct[key][common])
        errors[key + "_max_abs"] = delta.max(axis=0, initial=0).tolist()
        errors[key + "_p95_abs"] = (
            np.quantile(delta, 0.95, axis=0).tolist() if len(delta) else None
        )
    errors["common_qualified_rays"] = int(common.sum())
    errors["flat_program_mismatches"] = int(
        (flat_code(fields["uv"][common]) != flat_code(direct["uv"][common])).sum()
    )
    return errors


def colours(uv, valid):
    u, v = uv[:, 0], uv[:, 1]
    output = np.zeros((len(uv), 3), np.uint8)
    output[:, 0] = (np.mod(np.floor(np.abs(u) / 256) * 73, 256)).astype(np.uint8)
    output[:, 1] = (np.mod(np.abs(u), 1) * 255).astype(np.uint8)
    output[:, 2] = (np.mod(np.abs(v), 1) * 255).astype(np.uint8)
    output[~valid] = 0
    return output


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    args.output.mkdir(parents=True)
    files = [
        args.producer / "sample.json",
        args.producer / "sample.npz",
        args.storage / "scalar.npz",
    ]
    files.extend(sorted((args.storage / "recipes").iterdir()))
    hashes = {str(path): sha(path) for path in files}
    identity = json.loads((args.producer / "sample.json").read_text())[
        "prepared_source_ordinal"
    ]
    with np.load(args.storage / "scalar.npz", allow_pickle=False) as z:
        patches = restore_source(
            {key: z[key] for key in z.files},
            identity,
            source_recipe(args.storage / "recipes", identity),
        )
    with np.load(args.producer / "sample.npz", allow_pickle=False) as z:
        sources = {}
        for name in ("original", "candidate"):
            data = z[name + "_vertices"]
            sources[name] = {
                "positions": data[:, :3].copy(),
                "vertex_normal": data[:, 3:6].copy(),
                "uv": data[:, 6:8].copy(),
                "metres": data[:, 8:10].copy(),
                "colour": data[:, 10:14].copy(),
                "indices": z[name + "_faces"].copy(),
            }
    prepared = {name: prepare_source(source) for name, source in sources.items()}
    width = height = 96
    panel = Image.new("RGB", (width * 3, (height + 20) * 3), "white")
    painter, views = ImageDraw.Draw(panel), []
    for row, yaw in enumerate((0, 45, 120)):
        view = low_eye_view(
            sources["original"]["positions"],
            np.array([0, 1, 0]),
            np.array([1, 0, 0]),
            np.array([0, 0, -1]),
            720,
            3,
            yaw,
            0,
        )
        box = visible_pixel_bounds(sources["original"]["positions"], view)
        origins, directions = image_rays(view, box, width, height, supersample=1)
        depth, owner, work = intersect(patches, origins, directions)
        direct = direct_fields(patches, origins, directions, depth, owner)
        comparisons, sampled = {}, {}
        for name, source in sources.items():
            native_depth, faces = intersect_source(
                source, origins, directions, prepared=prepared[name]
            )
            fields = fields_at(source, origins, directions, native_depth, faces)
            comparisons[name] = compare(faces, fields, direct)
            sampled[name] = faces, fields
        unknown = (owner >= 0) & ~direct["qualified"]
        views.append(
            {
                "yaw_degrees": yaw,
                "rays": len(depth),
                "direct_hits": int((owner >= 0).sum()),
                "unqualified_domain_hits": int(unknown.sum()),
                "domain_work": work,
                "against": comparisons,
            }
        )
        faces, original = sampled["original"]
        common = (faces >= 0) & direct["qualified"]
        difference = np.zeros((len(depth), 3), np.uint8)
        difference[common & (flat_code(original["uv"]) != flat_code(direct["uv"]))] = [
            255,
            0,
            0,
        ]
        difference[unknown] = [255, 0, 255]
        panels = (
            colours(original["uv"], faces >= 0),
            colours(direct["uv"], direct["qualified"]),
            difference,
        )
        for col, (title, pixels) in enumerate(
            zip(("Triangle UV", "Domain UV", "Code/unknown"), panels, strict=True)
        ):
            painter.text(
                (col * width + 2, row * (height + 20) + 3),
                f"{yaw}° {title}",
                fill="black",
            )
            panel.paste(
                Image.fromarray(pixels.reshape(height, width, 3)),
                (col * width, row * (height + 20) + 20),
            )
    panel.save(args.output / "raw-fields.png")
    if hashes != {str(path): sha(path) for path in files}:
        raise ValueError("Material diagnostic inputs changed")
    report = {
        "source_ordinal": identity,
        "input_sha256": hashes,
        "views": views,
        "scope": "Finite CPU unsnapped-domain hit parameters and raw material coordinates versus triangle-interpolated source inputs. Refused material evaluations are unknown. No full material/PBR, derivatives, native raster, hidden surfaces, World, device time or general error bound.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--storage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
