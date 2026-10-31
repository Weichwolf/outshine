"""Depth-tested CPU geometry diagnostic, reusing the existing reference rasterizer.

Two-sided flat facet colours make geometry readable; no native/PBR image admission.
"""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from current_visibility import project, raster
from PIL import Image, ImageDraw
from shore_bank_inputs import roads
from shore_terrain_contact_study import clipped


def frame(vertices, width, height):
    low, high = vertices.min(axis=0), vertices.max(axis=0)
    centre, span = (low + high) / 2, max(high[:2] - low[:2])
    eye = centre + np.array([-0.8, -1.2, 0.75]) * span
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    basis = np.array([right, np.cross(right, forward), forward])
    return eye, basis, width * 0.8, width, height


def render(groups, view, bounds):
    triangles, kinds = [], []
    for kind, source in groups:
        result = clipped(source, bounds)
        triangles.extend(result)
        kinds.extend([kind] * len(result))
    triangles = np.array(triangles)
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    sizes = np.linalg.norm(normals, axis=1)
    # View clipping may create zero-area boundary pieces; source meshes stay unchanged.
    keep = sizes > 0
    triangles, kinds = triangles[keep], np.array(kinds)[keep]
    normals = normals[keep] / sizes[keep, None]
    sun = np.array([-0.3, -0.5, 0.8])
    sun /= np.linalg.norm(sun)
    palette = np.array(
        [[0.62, 0.48, 0.31], [0.68, 0.63, 0.51], [0.19, 0.39, 0.56], [0.31, 0.32, 0.33]]
    )
    colour = palette[kinds] * (0.35 + 0.65 * abs(normals @ sun))[:, None]
    points = triangles.reshape(-1, 3)
    faces = np.arange(len(points)).reshape(-1, 3)
    faces = np.vstack((faces, faces[:, ::-1]))
    colour = np.vstack((colour, colour))
    projected, z = project(points, view)
    depth, owner = raster(projected, z, faces, view[3], view[4], with_faces=True)
    image = np.full((*owner.shape, 3), [0.88, 0.91, 0.94])
    image[owner >= 0] = colour[owner[owner >= 0]]
    return Image.fromarray(np.rint(np.clip(image, 0, 1) * 255).astype(np.uint8)), {
        "source_view_triangles": len(triangles),
        "covered_pixels": int(np.count_nonzero(depth)),
        "profile": [view[3], view[4]],
    }


def run(args):
    if args.output.exists():
        raise ValueError("fresh system-temp output required")
    report = json.loads((args.prepared / "report.json").read_bytes())
    if (
        hashlib.sha256((args.inputs / "input.json").read_bytes()).hexdigest()
        != report["bank_receipt"]["manifest_sha256"]
    ):
        raise ValueError("diagnostic roads belong to another captured input")
    with np.load(args.prepared / "contact.npz", allow_pickle=False) as saved:
        data = dict(saved)
    with np.load(
        args.prepared / "bank/water-component.npz", allow_pickle=False
    ) as saved:
        water = dict(saved)
    original = data["original_vertices"]
    bounds = [*original[:, :2].min(axis=0), *original[:, :2].max(axis=0)]
    road, _, _ = roads(args.inputs, water["anchor"], bounds, include_vertical=True)
    common = [(2, water["vertices"][water["faces"]]), (3, road)]
    before = [(0, original[data["original_faces"]]), *common]
    after = [
        (0, data["terrain_vertices"][data["terrain_faces"]]),
        (1, data["body_vertices"][data["body_faces"]]),
        *common,
    ]
    view = frame(original, args.width, args.height)
    images, notes = zip(
        *(render(groups, view, bounds) for groups in (before, after)), strict=True
    )
    args.output.mkdir(parents=True)
    panel = Image.new("RGB", (2 * args.width, args.height + 30), "white")
    draw = ImageDraw.Draw(panel)
    for index, (image, title) in enumerate(
        zip(
            images,
            (
                "Original posting mesh reference",
                "Conformed banks / same roads and water",
            ),
            strict=True,
        )
    ):
        panel.paste(image, (index * args.width, 30))
        draw.text((index * args.width + 10, 8), title, fill="black")
    panel.save(args.output / "depth-view.png")
    record = {
        "renderer_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "geometry_commit": report["commit"],
        "panels": notes,
        "scope": __doc__
        + " Same explicit camera, roads, water and whole supplied XY window; no field/recipe changes. CPU projected Float64 reference with depth and two-sided visibility, no textures, shadows, transparent-water transport, native shader layout or realtime/device claim.",
    }
    (args.output / "report.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=960)
    parser.add_argument("--height", type=int, default=640)
    run(parser.parse_args())
