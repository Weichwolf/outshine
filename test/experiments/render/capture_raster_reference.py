"""Complete 2x2 reference frames with owned native vertex-input identities."""

import numpy as np
from check_building_capture_inputs import VERTEX
from current_visibility import project, raster
from native_capture_meshes import independent_depth, sample


def input_keys(meshes):
    """Comparison classes, not fabricated source/Part/Face identities."""
    registry, classes, packet_bytes = {}, {}, 0
    for name in ("Full", "Fine", "Captured"):
        source = meshes[name]
        vertices = [
            VERTEX.pack(*p, *uv, int(n), *colour, *metres)
            for p, uv, n, colour, metres in zip(
                source["positions"],
                source["uv"],
                source["normal"],
                source["colour"],
                source["metres"],
                strict=True,
            )
        ]
        ids = []
        for binding, triangle in zip(source["binding"], source["indices"], strict=True):
            packet = b"".join(vertices[i] for i in triangle)
            key = (int(binding[0]), int(binding[4]), packet)
            if key not in registry:
                registry[key] = len(registry)
                packet_bytes += len(packet)
            ids.append(registry[key])
        classes[name] = np.asarray(ids, np.int32)
    return classes, {
        "unique_owned_input_packets": len(registry),
        "packet_payload_bytes": packet_bytes,
        "scope": "Same source slot/stream and ordered Float32 vertex attributes; binding tables stay separate. Packet bytes exclude Python registry/tuple overhead; these keys do not qualify per-Piece material bindings.",
    }


def frame(source, view, *, counters=None):
    eye, basis, focal, width, height = view
    expanded = (eye, basis, 2 * focal, 2 * width, 2 * height)
    projected, z = project(source["positions"].astype(float), expanded)
    if (z <= 0.1).any():
        raise ValueError(
            "complete raster diagnostic requires geometry in front of near plane"
        )
    return raster(
        projected,
        z,
        source["indices"],
        2 * width,
        2 * height,
        with_faces=True,
        counters=counters,
    )


def keys_at(owners, classes):
    result = np.full(owners.shape, -1, np.int32)
    valid = owners >= 0
    result[valid] = classes[owners[valid]]
    return result


def errors(first, second, first_keys, second_keys):
    a, b = first[0] > 0, second[0] > 0
    common = a & b
    delta = abs(0.1 / first[0][common] - 0.1 / second[0][common])
    alpha_a = a.reshape(a.shape[0] // 2, 2, a.shape[1] // 2, 2).mean((1, 3))
    alpha_b = b.reshape(b.shape[0] // 2, 2, b.shape[1] // 2, 2).mean((1, 3))
    key_a, key_b = keys_at(first[1], first_keys), keys_at(second[1], second_keys)
    worst = int(np.flatnonzero(common)[delta.argmax()]) if len(delta) else None
    return {
        "reference_covered_subsamples": int(a.sum()),
        "candidate_covered_subsamples": int(b.sum()),
        "lost_subsamples": int((a & ~b).sum()),
        "gained_subsamples": int((~a & b).sum()),
        "alpha_l1_px": float(abs(alpha_a - alpha_b).sum()),
        "lost_half_coverage_pixels": int(((alpha_a >= 0.5) & (alpha_b < 0.5)).sum()),
        "gained_half_coverage_pixels": int(((alpha_a < 0.5) & (alpha_b >= 0.5)).sum()),
        "different_computed_depth_subsamples": int(
            (common & (first[0] != second[0])).sum()
        ),
        "max_forward_depth_difference_m": float(delta.max(initial=0)),
        "p95_forward_depth_difference_m": float(np.percentile(delta, 95))
        if len(delta)
        else None,
        "different_owned_vertex_input_subsamples": int(
            (common & (key_a != key_b)).sum()
        ),
        "worst_depth_difference_flat_subsample": worst,
    }


def controls(source, prepared, image, classes, view, changed, mandatory):
    valid = np.flatnonzero(image[0].ravel() > 0)
    empty = np.flatnonzero(image[0].ravel() == 0)
    selected = np.unique(
        np.r_[
            mandatory,
            valid[np.linspace(0, len(valid) - 1, min(32, len(valid))).astype(int)],
            empty[np.linspace(0, len(empty) - 1, min(16, len(empty))).astype(int)],
            changed[
                np.linspace(0, len(changed) - 1, min(32, len(changed))).astype(int)
            ],
        ]
    )
    y, x = np.unravel_index(selected, image[0].shape)
    eye, basis, focal, width, height = view
    directions = (
        np.column_stack(
            (
                ((x + 0.5) / 2 - width / 2) / focal,
                -((y + 0.5) / 2 - height / 2) / focal,
                np.ones(len(x)),
            )
        )
        @ basis
    )
    origins = np.broadcast_to(eye, directions.shape)
    depth, faces, _ = sample(source, prepared, origins, directions)
    expected = image[0].ravel()[selected]
    if not np.array_equal(depth < np.inf, expected > 0):
        raise ValueError("independent ray coverage differs from complete raster")
    both = np.isfinite(depth)
    np.testing.assert_allclose(0.1 / expected[both], depth[both], rtol=1e-10, atol=1e-8)
    brute = independent_depth(
        source["positions"][source["indices"]], origins, directions
    )
    np.testing.assert_array_equal(np.isfinite(depth), np.isfinite(brute))
    np.testing.assert_allclose(depth[both], brute[both], rtol=1e-10, atol=1e-8)
    raster_faces = image[1].ravel()[selected[both]]
    return {
        "rays": len(selected),
        "direct_triangle_pairs": len(selected) * len(source["indices"]),
        "max_ray_raster_depth_difference_m": float(
            abs(depth[both] - 0.1 / expected[both]).max(initial=0)
        ),
        "owned_input_control_disagreements": int(
            (classes[faces[both]] != classes[raster_faces]).sum()
        ),
    }


def compare_saved_rays(directory, name, frames, view):
    """Every original saved far-cohort subpixel ray, not selected good pixels."""
    saved = np.load(directory / f"{name}.npz")
    local = saved["directions"] @ view[1].T
    pixel = np.column_stack(
        (
            view[3] / 2 + view[2] * local[:, 0] / local[:, 2],
            view[4] / 2 - view[2] * local[:, 1] / local[:, 2],
        )
    )
    index = np.rint(2 * pixel - 0.5).astype(int)
    result = {}
    for representation, image in frames.items():
        actual = image[0][index[:, 1], index[:, 0]]
        reference = saved[representation + "_depth"]
        np.testing.assert_array_equal(actual > 0, np.isfinite(reference))
        both = actual > 0
        np.testing.assert_allclose(
            0.1 / actual[both], reference[both], rtol=1e-10, atol=1e-8
        )
        result[representation] = {
            "all_saved_rays": len(reference),
            "max_depth_difference_m": float(
                abs(0.1 / actual[both] - reference[both]).max(initial=0)
            ),
        }
    return result
