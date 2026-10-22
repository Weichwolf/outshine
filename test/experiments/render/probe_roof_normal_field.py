"""Same saved rays: normal lookup at profile hits versus original-surface hits."""

import argparse
import json
from pathlib import Path

import numpy as np
import trimesh
from probe_far_profile_fields import read_scene
from roof_normal_field import part_uv, sample_uv
from triangle_image_reference import image_rays


def angles(first, second):
    if not len(first):
        return None
    dot = np.einsum("ij,ij->i", first, second)
    error = np.degrees(np.arccos(np.clip(dot, -1, 1)))
    return {
        "samples": len(error),
        "maximum_deg": float(error.max()),
        "mean_deg": float(error.mean()),
        "over_one_degree": int((error > 1).sum()),
    }


def compare(name, args, field, old_report):
    original, view = read_scene(args.profiles / name / "original/scene.json")
    candidate, _ = read_scene(args.profiles / name / "profile/scene.json")
    scene = json.loads((args.field / name / "scene.json").read_text())
    basis = np.array(scene["roof_normal_field"]["normal_basis"])
    with np.load(args.field / name / "roof-uv.npz", allow_pickle=False) as archive:
        uv = archive["uv"]
    with np.load(args.depths / (name + ".npz"), allow_pickle=False) as archive:
        data = {key: archive[key] for key in archive.files}
    box = np.array(old_report["roi"])
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(view, box, width, height, 2)
    faces = data["candidate_face"]
    covered = faces >= 0
    hit = (
        origins[covered] + directions[covered] * data["candidate_depth"][covered, None]
    )
    triangles = candidate["positions"][candidate["indices"][faces[covered]]].astype(
        float
    )
    weights = trimesh.triangles.points_to_barycentric(triangles, hit)
    texcoord = np.einsum("ij,ijk->ik", weights, uv[faces[covered]])
    normals, valid = sample_uv(field, texcoord, basis)
    actual = data["candidate_normal"].copy()
    indices = np.flatnonzero(covered)[valid]
    actual[indices] = normals[valid]
    both = covered & (data["original_face"] >= 0)
    same = both & (data["original_source"] == data["candidate_source"])
    both &= candidate["native_binding"][np.maximum(faces, 0), 0] < 0
    ids = np.flatnonzero(both)
    old_binding = original["plan_binding"][data["original_face"][both]]
    new_binding = candidate["plan_binding"][faces[both]]
    roof = np.zeros(len(faces), bool)
    roof[ids] = (
        np.all(old_binding[:, :2] == new_binding[:, :2], axis=1)
        & (old_binding[:, 2] == 1)
        & (new_binding[:, 2] == 1)
    )
    # Infer the common display origin from the verified source frame and one
    # unmodified metadata association, supplied explicitly by the frozen adapter.
    centre = np.array(scene["roof_normal_field"]["parent_origin"])
    true_point = origins[roof] + directions[roof] * data["original_depth"][roof, None]
    parent = true_point @ basis + centre
    row_map = {tuple(key): i for i, key in enumerate(field["keys"])}
    field_rows = np.array(
        [row_map[tuple(k)] for k in candidate["plan_binding"][faces[roof], :2]]
    )
    original_uv = np.empty((len(parent), 2))
    for row in np.unique(field_rows):
        selected = field_rows == row
        original_uv[selected] = part_uv(field, int(row), parent[selected])
    at_original, eligible = sample_uv(field, original_uv, basis)
    reference = data["original_normal"]
    return {
        "pose": name,
        "normal_overrides": len(indices),
        "all_same_source_flat": angles(reference[same], data["candidate_normal"][same]),
        "all_same_source_field": angles(reference[same], actual[same]),
        "same_source_part_roof_flat": angles(
            reference[roof], data["candidate_normal"][roof]
        ),
        "same_source_part_roof_field": angles(reference[roof], actual[roof]),
        "same_roof_field_at_original_hit": angles(
            reference[roof][eligible], at_original[eligible]
        ),
        "scope": "Original and profile geometry/depth/coverage are unchanged saved results. Point-normal control only; no Blender filtering/BRDF/shadow/native admission.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profiles", type=Path, required=True)
    parser.add_argument("--field", type=Path, required=True)
    parser.add_argument("--depths", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    old = {r["view"]: r for r in json.loads((args.depths / "report.json").read_text())}
    with np.load(args.field / "field.npz", allow_pickle=False) as archive:
        field = {k: archive[k] for k in archive.files}
    result = []
    for name in ["static-020", "moving-020", "moving-021", "near-control"]:
        row = compare(name, args, field, old[name])
        result.append(row)
        print(json.dumps(row), flush=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
