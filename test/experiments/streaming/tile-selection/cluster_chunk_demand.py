"""Replay bounded cluster chunks of a native mesh snapshot over benchmark motion."""

import argparse
import json
from pathlib import Path
from time import perf_counter

import numpy as np


def rows(directory, name, width, dtype):
    return np.fromfile(directory / (name + ".bin"), dtype).reshape(-1, width)


def affine_bounds(models, packed):
    linear = models[:, :3, :3]
    translation = models[:, :3, 3]
    points = packed[:, :3]
    centre = np.einsum("nij,nj->ni", linear, points) + translation
    gram = np.swapaxes(linear, 1, 2) @ linear
    scale = np.sqrt(np.abs(gram).sum(axis=2).max(axis=1))
    error = 16 * np.finfo(np.float32).eps
    reach = np.einsum("nij,nj->ni", np.abs(linear), np.abs(points) + packed[:, 3:])
    guards = error * (reach + np.abs(translation) + 1)
    radius = (packed[:, 3] * scale + np.linalg.norm(guards, axis=1)) * (1 + error)
    return centre, radius


def view_planes(report, metadata):
    motion = report["motion"]
    pose = motion["start"]
    start = np.asarray(pose["world_from_camera"]).reshape(4, 4)[:3, :3]
    initial_eye = np.asarray(pose["position_m"])
    finish = motion["finish"]
    final_eye = np.asarray(finish["position_m"])
    final_axes = np.asarray(finish["world_from_camera"]).reshape(4, 4)[:3, :3]
    native_eye = np.asarray(metadata["eye_from_anchor_m"])
    native_rotation = final_axes.T @ np.asarray(metadata["camera_axes"])
    velocity = np.asarray(motion["linear_velocity_mps"])
    spin = np.deg2rad(motion["angular_velocity_deg_s"])
    angular_speed = np.linalg.norm(spin)
    spin_axis = spin / angular_speed if angular_speed else np.zeros(3)
    slope_y = np.tan(np.deg2rad(pose["perspective_fov_deg"]) * 0.5)
    width, height = metadata["viewport_px"]
    slope_x = slope_y * width / height
    for frame in range(motion["frames"]):
        seconds = (frame + 1) * motion["time_step_s"]
        angle = angular_speed * seconds
        rotated = (
            start * np.cos(angle)
            + np.cross(spin_axis, start) * np.sin(angle)
            + (start @ spin_axis)[:, None] * spin_axis * (1 - np.cos(angle))
        )
        right, up, backward = rotated @ native_rotation
        forward = -backward
        horizontal = forward * slope_x * (1 + 1 / width)
        vertical = forward * slope_y * (1 + 1 / height)
        normals = [
            horizontal + right,
            horizontal - right,
            vertical + up,
            vertical - up,
            forward,
        ]
        offset = [0, 0, 0, 0, -pose["near_m"]]
        if pose["far_m"] > 0:
            normals.append(backward)
            offset.append(pose["far_m"])
        normals = np.asarray(normals)
        length = np.linalg.norm(normals, axis=1)
        eye = (
            initial_eye + velocity * seconds - final_eye
        ) @ native_rotation + native_eye
        yield (
            eye,
            normals / length[:, None],
            np.asarray(offset) / length,
        )


def grouping(counts, owners, ordinal, bounds, sizes):
    low, high = bounds
    chunks = {}
    for size in sizes:
        starts = np.flatnonzero(ordinal % size == 0)
        groups = owners[starts]
        members = np.minimum(counts[groups] - ordinal[starts], size)
        lo = np.minimum.reduceat(low, starts)
        hi = np.maximum.reduceat(high, starts)
        chunks[size] = groups, members, lo, hi
    return chunks


def count_work(snapshot, sizes):
    def read(name, width, dtype=np.uint32):
        return rows(snapshot, name, width, dtype)

    meta = json.loads((snapshot / "geometry.json").read_text())
    report = json.loads((snapshot / "benchmark.json").read_text())
    roots = read("roots", 12)
    templates = read("templates", 4)
    spheres = read("cluster_spheres", 12, np.float32)
    placements = read("placements", 16, np.float32).reshape(-1, 4, 4).transpose(0, 2, 1)
    model = placements[roots[:, 8]].astype(np.float64)
    counts = roots[:, 5].astype(np.int64)
    owners = np.repeat(np.arange(len(roots)), counts)
    ordinal = np.arange(len(owners)) - roots[owners, 6]
    assert (ordinal >= 0).all() and (ordinal < counts[owners]).all()
    leaf_ids = roots[owners, 4] + ordinal
    leaf_templates = templates[leaf_ids]
    leaf_spheres = spheres[leaf_templates[:, 0]]
    terminal = (leaf_spheres[:, 8] == 0) & (leaf_spheres[:, 9] == np.float32(3.0e38))
    first_index, index_count = leaf_templates[:, 1:3].astype(np.int64).T
    discontinuity = np.r_[
        False,
        (ordinal[1:] != 0) & (first_index[1:] != first_index[:-1] + index_count[:-1]),
    ]
    contiguous = np.bincount(owners, weights=discontinuity, minlength=len(roots)) == 0
    index_words = np.bincount(owners, weights=index_count, minlength=len(roots)).astype(
        np.int64
    )
    assert (index_words == roots[:, 10]).all()
    root_position, root_radius = affine_bounds(
        model, roots[:, :4].copy().view(np.float32)
    )
    centres, radii = affine_bounds(model[owners], leaf_spheres[:, :4])
    low, high = centres - radii[:, None], centres + radii[:, None]
    chunks = grouping(counts, owners, ordinal, (low, high), sizes)
    affine = (model[:, 3, :] == [0, 0, 0, 1]).all(axis=1) & np.isfinite(model).all(
        axis=(1, 2)
    )
    bounded = ((roots[:, 11] & 3) == 1) & affine
    main = (roots[:, 11] & 4) == 0
    measurements = []
    complete_roots = []
    direct_words = []
    indirect_words = []
    final_root_mask = None
    for eye, normals, offset in view_planes(report, meta):
        shift = -eye
        # Widen near the boundary of either selection. CPU/GPU may round in opposite directions.
        plane_margin = (
            32
            * np.finfo(np.float32).eps
            * (np.abs(normals).sum(axis=1) * (1 + np.abs(shift).max()) + np.abs(offset))
        )
        plane_offset = normals @ shift + offset
        eye_radius_guard = np.linalg.norm(np.abs(shift)) * 16 * np.finfo(np.float32).eps
        root_distances = root_position @ normals.T + plane_offset
        in_frustum = (
            root_distances >= -root_radius[:, None] - eye_radius_guard - plane_margin
        ).all(axis=1)
        root_kept = main & (~bounded | in_frustum)
        final_root_mask = root_kept
        old = int(counts[root_kept].sum())
        leaf_distances = centres @ normals.T + plane_offset
        leaf_margin = 32 * np.finfo(np.float32).eps * (1 + np.abs(centres).sum(axis=1))
        leaf_visible = (
            leaf_distances
            >= -radii[:, None] - eye_radius_guard - leaf_margin[:, None] - plane_margin
        ).all(axis=1)
        root_margin = (
            32 * np.finfo(np.float32).eps * (1 + np.abs(root_position).sum(axis=1))
        )
        inside = (
            bounded
            & root_kept
            & (
                root_distances
                >= root_radius[:, None]
                + eye_radius_guard
                + root_margin[:, None]
                + plane_margin
            ).all(axis=1)
        )
        all_visible = np.bincount(
            owners, weights=terminal & leaf_visible, minlength=len(roots)
        )
        accepted = inside & (all_visible == counts)
        complete_roots.append(int(counts[accepted].sum()))
        direct_words.append(int(index_words[accepted & contiguous].sum()))
        indirect_words.append(int(index_count[root_kept[owners] & leaf_visible].sum()))
        new = {}
        for size, (groups, members, lo, hi) in chunks.items():
            vertices = hi @ np.maximum(normals.T, 0) + lo @ np.minimum(normals.T, 0)
            margin = (
                32
                * np.finfo(np.float32).eps
                * (1 + np.maximum(np.abs(lo), np.abs(hi)).sum(axis=1))
            )
            in_chunk_frustum = (
                vertices + plane_offset
                >= -eye_radius_guard - margin[:, None] - plane_margin
            ).all(axis=1)
            testable = root_kept[groups] & bounded[groups] & (counts[groups] > size)
            rejected = testable & ~in_chunk_frustum
            assert not leaf_visible[np.repeat(rejected, members)].any()
            new[size] = [int(testable.sum()), int(members[rejected].sum())]
        measurements.append((old, new))
    return {
        "roots": len(roots),
        "cluster_addresses": len(owners),
        "frames": len(measurements),
        "root_cull_leaf_tests": sum(accepted for accepted, _ in measurements),
        "terminal_leaves_accepted_by_entire_root": sum(complete_roots),
        "source_index_words_from_contiguous_accepted_roots": sum(direct_words),
        "index_words_after_leaf_frustum": sum(indirect_words),
        "final_root_mask_mismatches": int(
            np.count_nonzero(final_root_mask != (read("visible", 1).ravel() != 0))
        ),
        "chunk_tests_and_avoided_leaf_tests": {
            size: np.sum(
                [by_size[size] for _, by_size in measurements], axis=0
            ).tolist()
            for size in sizes
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("--sizes", nargs="+", type=int, default=(16, 32, 64))
    args = parser.parse_args()
    if any(size < 1 for size in args.sizes):
        parser.error("chunk sizes must be positive")
    started = perf_counter()
    result = count_work(args.snapshot, args.sizes)
    result["elapsed_ms"] = (perf_counter() - started) * 1000
    print(json.dumps(result))


if __name__ == "__main__":
    main()
