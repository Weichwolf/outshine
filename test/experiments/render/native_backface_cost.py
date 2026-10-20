"""Compare early backface representations on fixed selected native geometry."""

import argparse
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from native_visibility_cost import load
from native_visibility_data import digest

ROOT = Path(__file__).resolve().parents[3]
STATE_INPUTS = {
    name + ".bin"
    for name in (
        "vertices",
        "indices",
        "jobs",
        "kept",
        "arguments",
        "placements",
        "uniform",
        "spheres",
    )
} | {"draws.json"}


def load_batch_states(source, path):
    data = json.loads(path.read_text())
    if data["schema"] != 1:
        raise ValueError("unknown native state schema")
    if set(data["input_sha256"]) != STATE_INPUTS:
        raise ValueError("native state is missing input identities")
    for name, expected in data["input_sha256"].items():
        if Path(name).name != name or digest(source / name) != expected:
            raise ValueError("native state does not belong to these input bytes")
    states = np.zeros((data["rows"], 3), dtype=np.uint32)
    cursor = 0
    for begin, end, kind, culls_back, rigid in data["runs"]:
        if (
            begin != cursor
            or not begin < end <= len(states)
            or kind not in range(5)
            or culls_back not in (0, 1)
            or rigid not in (0, 1)
        ):
            raise ValueError("invalid or incomplete native state ranges")
        states[begin:end] = kind, culls_back, rigid
        cursor = end
    if cursor != len(states):
        raise ValueError("native state ranges do not cover all batches")
    return states


def unit_normals(points):
    values = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    lengths = np.linalg.norm(values, axis=1)
    return values / np.where(lengths > 0, lengths, 1)[:, None]


def cone_rejects(points, normals, eye, margin):
    if not np.isfinite(normals).all() or ((normals * normals).sum(axis=1) < 0.5).any():
        return False
    axis = normals.sum(axis=0)
    length = np.linalg.norm(axis)
    if length < 1e-12:
        return False
    axis /= length
    dots = normals @ axis
    cutoff = float(dots.min())
    if cutoff <= 1e-6:
        return False
    centre = (points.min(axis=(0, 1)) + points.max(axis=(0, 1))) * 0.5
    offset = np.max(np.einsum("ij,ij->i", normals, centre - points[:, 0]) / dots)
    apex = centre - axis * offset
    direction = eye - apex
    along = float(direction @ axis)
    return bool(
        along < -margin
        and along * along
        > np.dot(direction, direction) * max(0, 1 - cutoff * cutoff)
        + margin * (2 * abs(along) + margin)
    )


def cell_rays(direction, grid):
    major = int(np.abs(direction).argmax())
    distance = abs(direction[major])
    if distance == 0:
        return None, distance
    other = [(major + 1) % 3, (major + 2) % 3]
    uv = direction[other] / distance
    cell = np.minimum(np.floor((uv + 1) * 0.5 * grid).astype(int), grid - 1)
    low, high = cell * 2 / grid - 1, (cell + 1) * 2 / grid - 1
    rays = np.zeros((4, 3))
    rays[:, major] = 1 if direction[major] >= 0 else -1
    rays[:, other[0]] = [low[0], high[0], low[0], high[0]]
    rays[:, other[1]] = [low[1], low[1], high[1], high[1]]
    return rays, distance


def face_rejects(normals, offsets, rays, cutoff, margin):
    support = (normals @ rays.T).max(axis=1)
    valid = (normals * normals).sum(axis=1) >= 0.5
    return valid & (support < 0) & (offsets + cutoff * support < -margin)


def compare(snapshot, states, spheres, limits=(128, 64, 32, 16, 8)):
    projection = snapshot.clip[[0, 1, 3]]
    eye = np.linalg.solve(projection[:, :3], -projection[:, 3]) - snapshot.shift
    inverse = np.linalg.inv(snapshot.models)
    eyes = np.einsum("nij,j->ni", inverse, np.r_[eye, 1])[:, :3]
    orientations = np.sign(np.linalg.det(snapshot.models[:, :3, :3]))
    cones = [
        {
            "limit": limit,
            "groups": 0,
            "rejected_triangles": 0,
            "all_backface_group_triangles": 0,
        }
        for limit in limits
    ]
    masks = [
        {"grid": grid, "cutoff_radii": k, "queried_jobs": 0, "rejected_triangles": 0}
        for grid in (1, 2)
        for k in (0, 1, 2, 4, 8)
    ]
    eligible, backfaces, total, false_rejections, bad_spheres = 0, 0, 0, 0, 0
    for index, (node, row, first, count) in enumerate(snapshot.jobs):
        first, count = int(first), int(count)
        points = np.asarray(
            snapshot.vertices[snapshot.indices[first : first + count]], dtype=np.float64
        ).reshape(-1, 3, 3)
        total += len(points)
        if not states[row, 1] or not states[row, 2]:
            continue
        normals = unit_normals(points) * orientations[index]
        margin = (
            64
            * np.finfo(np.float32).eps
            * (np.linalg.norm(eyes[index]) + np.max(np.abs(points)) + 1)
        )
        signed = np.einsum("ij,ij->i", normals, eyes[index] - points[:, 0])
        back = signed < -margin
        geometric_back = ((normals * normals).sum(axis=1) >= 0.5) & (signed < 0)
        eligible += len(points)
        backfaces += int(geometric_back.sum())
        for result in cones:
            for at in range(0, len(points), result["limit"]):
                last = at + result["limit"]
                result["groups"] += 1
                if geometric_back[at:last].all():
                    result["all_backface_group_triangles"] += len(points[at:last])
                if cone_rejects(points[at:last], normals[at:last], eyes[index], margin):
                    result["rejected_triangles"] += len(points[at:last])
                    false_rejections += int((~back[at:last]).sum())
        centre, radius = spheres[node, :3], float(spheres[node, 3])
        if np.max(np.linalg.norm(points - centre, axis=2)) > radius + margin:
            bad_spheres += 1
            continue
        offsets = np.einsum("ij,ij->i", normals, centre - points[:, 0])
        for result in masks:
            rays, distance = cell_rays(eyes[index] - centre, result["grid"])
            cutoff = result["cutoff_radii"] * radius
            if rays is None or distance < cutoff + margin:
                continue
            dropped = face_rejects(normals, offsets, rays, cutoff, margin)
            result["queried_jobs"] += 1
            result["rejected_triangles"] += int(dropped.sum())
            false_rejections += int((dropped & ~back).sum())
    return {
        "scope": "Fixed selected geometry, Double predicates; no GPU/frame/movement admission",
        "triangles": total,
        "eligible_rigid_triangles": eligible,
        "eligible_geometric_backfaces": backfaces,
        "false_rejections_vs_double_margin": false_rejections,
        "sphere_enclosure_failures": bad_spheres,
        "cones": cones,
        "direction_masks": masks,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--batch-states", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    roots = {Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve()}
    if output.is_relative_to(ROOT) or not any(output.is_relative_to(p) for p in roots):
        parser.error("use a system-temp output outside the checkout")
    began = time.perf_counter()
    source = args.input.resolve()
    states = load_batch_states(source, args.batch_states)
    snapshot = load(source)
    if len(states) != len(np.fromfile(source / "arguments.bin", dtype="<u4")) // 5:
        parser.error("native state row count differs from the arguments")
    spheres = np.fromfile(source / "spheres.bin", dtype="<f4").reshape(-1, 3, 4)[:, 0]
    result = compare(snapshot, states, spheres)
    result.update(
        wall_s=time.perf_counter() - began,
        batch_states_sha256=digest(args.batch_states),
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"triangles": result["triangles"], "report": str(output)}))
    return int(result["false_rejections_vs_double_margin"] != 0)


if __name__ == "__main__":
    raise SystemExit(main())
