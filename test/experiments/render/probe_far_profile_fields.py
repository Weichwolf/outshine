"""Same pixel lattice against saved complete original/profile assets; no source cook."""

import argparse
import json
from pathlib import Path

import numpy as np
from native_capture_meshes import fields_at
from triangle_image_reference import (
    image_rays,
    intersect_source,
    prepare_source,
    projected_bounds,
)


def read_scene(path):
    data = json.loads(path.read_text())
    with np.load(path.parent / data["mesh"], allow_pickle=False) as archive:
        mesh = {key: archive[key] for key in archive.files}
    mesh["vertex_normal"] = mesh["shading_normal"]
    view = (
        np.array(data["eye"]),
        np.array(data["camera_basis"]),
        data["focal_px"],
        data["width"],
        data["height"],
    )
    return mesh, view


def hit_codes(mesh, faces):
    """Decode categorical vertex tags before barycentric roundoff at a hit."""
    u = mesh["uv"][:, 0]
    codes = np.where(u < 0, u, np.floor(u / 256)).astype(np.int64)
    per_triangle = codes[mesh["indices"]]
    if not np.all(per_triangle == per_triangle[:, :1]):
        raise ValueError("Categorical material seam crosses an input triangle")
    result = np.full(len(faces), np.iinfo(np.int64).min, np.int64)
    covered = faces >= 0
    result[covered] = per_triangle[faces[covered], 0]
    return result


def compare(directory):
    original, view = read_scene(directory / "original/scene.json")
    candidate, other = read_scene(directory / "profile/scene.json")
    if not all(np.array_equal(a, b) for a, b in zip(view, other, strict=True)):
        raise ValueError("Different reference/candidate camera")
    fields = ["positions", "indices", "uv", "normal", "colour", "metres", "source_id"]
    if all(np.array_equal(original[k], candidate[k]) for k in fields):
        return {
            "all_draw_fields_equal": True,
            "scope": "Complete identical saved geometry/fields; no redundant near-field ray run",
        }, None
    bounds = np.array(
        [projected_bounds(m["positions"], view) for m in [original, candidate]]
    )
    box = np.stack((np.floor(bounds[:, 0].min(0)), np.ceil(bounds[:, 1].max(0))))
    box[0] = np.maximum(box[0], 0)
    box[1] = np.minimum(box[1], view[3:])
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(view, box, width, height, 2)
    outputs = []
    for mesh in [original, candidate]:
        index = prepare_source(mesh)
        depth, faces = intersect_source(mesh, origins, directions, prepared=index)
        data = fields_at(mesh, origins, directions, depth, faces, rgba=True)
        owner = np.full(len(faces), -1, np.int32)
        covered = faces >= 0
        owner[covered] = mesh["source_id"][faces[covered]]
        outputs.append((depth, faces, data, owner, hit_codes(mesh, faces)))
    (a, af, aa, ao, ac), (b, bf, bb, bo, bc) = outputs
    both = np.isfinite(a) & np.isfinite(b)
    same = both & (ao == bo)
    dots = np.einsum("ij,ij->i", aa["normal"][same], bb["normal"][same])
    result = {
        "all_draw_fields_equal": False,
        "roi": box.tolist(),
        "ray_samples": len(a),
        "original_hits": int(np.isfinite(a).sum()),
        "candidate_hits": int(np.isfinite(b).sum()),
        "lost_samples": int((np.isfinite(a) & ~np.isfinite(b)).sum()),
        "added_samples": int((~np.isfinite(a) & np.isfinite(b)).sum()),
        "different_source_samples": int((both & (ao != bo)).sum()),
        "maximum_same_source_depth_difference_m": float(np.abs(a[same] - b[same]).max())
        if same.any()
        else None,
        "maximum_same_source_normal_angle_deg": float(
            np.degrees(np.arccos(np.clip(dots, -1, 1))).max()
        )
        if same.any()
        else None,
        "same_source_encoded_category_mismatches": int((ac[same] != bc[same]).sum()),
        "same_source_samples": int(same.sum()),
        "scope": "Opaque Double pixel-centre triangle/Rtree control with four fixed samples/pixel. Categories include encoded source identity; native vertex shader/PBR/filtering/MSAA/shadows not qualified.",
    }
    arrays = {
        "original_depth": a,
        "candidate_depth": b,
        "original_face": af,
        "candidate_face": bf,
        "original_source": ao,
        "candidate_source": bo,
        "original_normal": aa["normal"],
        "candidate_normal": bb["normal"],
        "original_uv": aa["uv"],
        "candidate_uv": bb["uv"],
        "original_code": ac,
        "candidate_code": bc,
    }
    return result, arrays


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for name in ["static-020", "moving-020", "moving-021", "near-control"]:
        result, arrays = compare(args.input / name)
        rows.append({"view": name} | result)
        if arrays is not None:
            np.savez_compressed(args.output / (name + ".npz"), **arrays)
        print(name, json.dumps(result), flush=True)
    (args.output / "report.json").write_text(json.dumps(rows, indent=2) + "\n")
