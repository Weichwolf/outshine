"""Whole-source geometry opponents for coupled, prefilter C/T composition.

This CPU opaque/Lambert diagnostic uses unchanged delivered native geometry and
source colours. It is independent of the native PBR/reference images and does
not qualify the runtime field, a new pose domain or continuous pixel integration.
"""

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from compare_building_impostors import native_axes
from compare_directed_groups import retained_subset
from field_composition import (
    OriginalBounds,
    compose_points,
    depth_order,
    forward_interval,
)
from field_view_sampling import ImageView
from native_building_cell_reference import load_cells
from parent_surface_inputs import open_parent
from PIL import Image, ImageDraw
from triangle_image_reference import (
    average_samples,
    image_rays,
    intersect_source,
    prepare_source,
    visible_pixel_bounds,
)

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-full-view"


def bound(source, name):
    positions = source["positions"]
    return OriginalBounds(tuple(positions.min(0)), tuple(positions.max(0)), name)


def load_original(cells):
    request = json.loads((DATA / "source-plane-request.json").read_text())
    basis = np.asarray([request[k] for k in ("east", "north", "up")])
    with open_parent(DATA / "original-inputs.tar.gz") as parent:
        ids = [
            i
            for i, record in enumerate(parent.plans["records"])
            if record["cell"] in cells
        ]
    source, receipt = load_cells(
        DATA / "native-cells/manifest.json",
        DATA / "original-inputs.tar.gz",
        set(cells),
        {"basis": basis, "anchor": basis @ request["origin"], "selected_sources": ids},
    )
    if receipt["unresolved_originals"]:
        raise ValueError(
            "Missing originals must remain an explicit unrendered remainder"
        )
    # Match the old native original mesh export's axis conversion and Float32 store.
    source["positions"] = native_axes(source["positions"]).astype(np.float32)
    source["vertex_normal"] = native_axes(source["vertex_normal"]).astype(np.float32)
    return source, receipt


def partitions(source, view):
    identities = np.unique(source["binding"][:, 1])
    intervals = []
    for identity in identities:
        part = retained_subset(source, [identity])
        intervals.append(
            forward_interval(bound(part, str(identity)), view.eye, view.basis[2])
        )
    intervals = np.asarray(intervals)
    middle = float(np.median(intervals.mean(1)))
    # A declared 10m empty control band, not a depth tolerance or quality threshold.
    front = identities[intervals[:, 1] < middle - 5]
    back = identities[intervals[:, 0] > middle + 5]
    remainder = np.setdiff1d(identities, np.r_[front, back])
    parity = identities[np.arange(len(identities)) % 2 == 0]
    return [
        ("ordered-complete-sources", front, back, remainder),
        (
            "interleaved-complete-sources",
            parity,
            np.setdiff1d(identities, parity),
            np.array([], int),
        ),
    ]


def point_image(source, rays):
    prepared = prepare_source(source)
    depth, faces = intersect_source(source, *rays, prepared=prepared)
    rgba = np.zeros((len(depth), 4), np.float64)
    hit = np.isfinite(depth)
    normal = prepared["normal"].copy()
    normal /= np.maximum(np.linalg.norm(normal, axis=1), np.finfo(float).tiny)[:, None]
    sun = np.array([0.3, 0.8, 0.5])
    sun /= np.linalg.norm(sun)
    shade = 0.2 + 0.8 * np.maximum(normal @ sun, 0)
    colour = source["colour"][source["indices"], :3].mean(1)
    rgba[hit, :3] = colour[faces[hit]] * shade[faces[hit], None]
    rgba[hit, 3] = 1
    return rgba, depth


def errors(reference, value):
    delta = abs(reference - value)
    return {
        "rgb_l1": float(delta[:, :, :3].sum()),
        "coverage_l1": float(delta[:, :, 3].sum()),
        "rgba_max_abs": float(delta.max(initial=0)),
    }


def compare_case(source, view, row, samples_axis, output):
    name, front_ids, back_ids, remainder = row
    if not len(front_ids) or not len(back_ids):
        raise ValueError("A nonempty pair of complete original groups is required")
    pair_ids = np.r_[front_ids, back_ids]
    all_ids = np.unique(source["binding"][:, 1])
    if len(np.unique(np.r_[pair_ids, remainder])) != len(all_ids) or len(
        pair_ids
    ) + len(remainder) != len(all_ids):
        raise ValueError(
            "Every original source must be exactly one group or retained original"
        )
    pair, front, back = [
        retained_subset(source, ids) for ids in (pair_ids, front_ids, back_ids)
    ]
    camera = (
        np.asarray(view.eye),
        np.asarray(view.basis),
        view.focal_px,
        view.width,
        view.height,
    )
    box = visible_pixel_bounds(pair["positions"], camera)
    width, height = map(int, box[1] - box[0])
    rays_count = width * height * samples_axis**2
    if min(width, height) < 1 or rays_count > 262144:
        raise ValueError("Bounded nonempty ray patch required before expansion")
    rays = image_rays(camera, box, width, height, samples_axis)
    original, _ = point_image(pair, rays)
    a, za = point_image(front, rays)
    b, zb = point_image(back, rays)
    shape = (height * samples_axis, width * samples_axis, 4)
    ab = compose_points(a.reshape(shape), b.reshape(shape))
    ba = compose_points(b.reshape(shape), a.reshape(shape))
    reduce = lambda image: average_samples(image, width, height, samples_axis)
    truth, af, bf, joint_ab, joint_ba = map(reduce, (original, a, b, ab, ba))
    separate = af + (1 - af[:, :, 3:4]) * bf
    order = depth_order(
        bound(front, "front"), bound(back, "back"), view.eye, view.basis[2]
    )
    common = np.isfinite(za) & np.isfinite(zb)
    if order[0] == "front_before_back" and not np.array_equal(
        original, ab.reshape(-1, 4)
    ):
        raise ValueError(
            "Admitted complete-hull order disagrees with original triangle opponent"
        )
    np.savez_compressed(
        output / (name + ".npz"),
        original=truth,
        joint_ab=joint_ab,
        joint_ba=joint_ba,
        separate=separate,
        point_front=a,
        point_back=b,
        front_depth=za,
        back_depth=zb,
    )
    panel(truth, joint_ab, joint_ba, separate, output / (name + ".png"))
    return {
        "name": name,
        "complete_source_ids": {
            "front": front_ids.tolist(),
            "back": back_ids.tolist(),
            "retained_originals": remainder.tolist(),
        },
        "pair_triangles": len(pair["indices"]),
        "pair_array_bytes": sum(v.nbytes for v in pair.values()),
        "view": vars(view),
        "pixel_patch_origin": box[0].tolist(),
        "pixel_patch_shape": [height, width],
        "samples_axis": samples_axis,
        "rays_per_group": rays_count,
        "group_depth_order": order,
        "full_original_depth_intervals": [
            forward_interval(bound(s, n), view.eye, view.basis[2])
            for s, n in ((front, "front"), (back, "back"))
        ],
        "common_hit_rays": int(common.sum()),
        "front_nearer_rays": int((common & (za < zb)).sum()),
        "back_nearer_rays": int((common & (zb < za)).sum()),
        "equal_depth_rays": int((common & (za == zb)).sum()),
        "joint_front_before_back": errors(truth, joint_ab),
        "joint_back_before_front": errors(truth, joint_ba),
        "separate_filter_front_before_back": errors(truth, separate),
        "composition_work": {
            "point_rgba_pairs": rays_count,
            "output_pixels": width * height,
            "minimum_input_bytes": a.nbytes + b.nbytes,
            "owned_combined_bytes": ab.nbytes,
            "known_point_array_copy_lower_bytes": 0,
            "total_copy_bytes": None,
        },
    }


def panel(truth, ab, ba, separate, path):
    images = (truth, ab, ba, separate)
    names = (
        "original triangle rays",
        "joint A over B then filter",
        "joint B over A then filter",
        "separate filters, A over B",
    )
    canvas = Image.new("RGB", (1280, 340), "#eeeeee")
    draw = ImageDraw.Draw(canvas)
    for index, (value, name) in enumerate(zip(images, names, strict=True)):
        rgb = value[:, :, :3] + (1 - value[:, :, 3:4]) * [0.08, 0.12, 0.18]
        encoded = np.round(np.clip(rgb, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)
        image = Image.fromarray(encoded).resize((300, 280), Image.Resampling.NEAREST)
        canvas.paste(image, (index * 320, 40))
        draw.text((index * 320 + 4, 10), name, fill="black")
    draw.text(
        (4, 325),
        "CPU diagnostic; enlarged pixel patch; source vertex colours + Lambert; not native PBR",
        fill="black",
    )
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--camera", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", default=[25, 33])
    parser.add_argument("--samples-axis", type=int, default=16)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if (
        not 1 <= args.samples_axis <= 32
        or not args.output.resolve().is_relative_to(
            Path(tempfile.gettempdir()).resolve()
        )
        or args.output.resolve().is_relative_to(ROOT)
    ):
        raise ValueError(
            "Bounded samples and an explicit system-temp output are required"
        )
    query = json.loads(args.camera.read_text())
    view = ImageView(
        tuple(query["eye"]), tuple(map(tuple, query["basis"])), query["focal_px"]
    )
    args.output.mkdir(parents=True, exist_ok=False)
    source, receipt = load_original(args.cells)
    rows = [
        compare_case(source, view, row, args.samples_axis, args.output)
        for row in partitions(source, view)
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "producer_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", "lib", "test/experiments"],
                text=True,
            )
        ),
        "camera_sha256": hashlib.sha256(args.camera.read_bytes()).hexdigest(),
        "source": receipt,
        "rows": rows,
        "scope": "Unchanged native original positions, complete IDs and colours; regrouping only. Ordered control excludes explicit retained-original IDs from the pair image. Opaque CPU triangle/Lambert finite-sample diagnostic, not native PBR, raw-capture error, continuous integration, World admission, device time, p99 or UMA acceptance.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            [{k: v for k, v in r.items() if k != "complete_source_ids"} for r in rows]
        )
    )


if __name__ == "__main__":
    main()
