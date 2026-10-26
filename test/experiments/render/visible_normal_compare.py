"""Matched opaque controls and complete-pixel comparison for shared measures."""

import time

import numpy as np
from opaque_area_reference import evaluate, pixel_grid, quadrature
from triangle_image_reference import average_samples, image_rays, intersect_source
from viewcell_visibility import select_support
from visible_normal_query import query


def display(rgb):
    return 1 - np.exp(-4 * np.maximum(rgb, 0))


def metrics(reference, candidate):
    delta = candidate - reference
    rms, baseline = np.sqrt(np.mean(delta**2)), np.sqrt(np.mean(reference**2))
    return {
        "linear_rmse": float(rms),
        "linear_max_abs": float(abs(delta).max(initial=0)),
        "linear_relative_rms": float(rms / baseline) if baseline else None,
        "display_rmse": float(
            np.sqrt(np.mean((display(candidate) - display(reference)) ** 2))
        ),
        "display_max_abs": float(
            abs(display(candidate) - display(reference)).max(initial=0)
        ),
    }


def geometry_control(source, records, path):
    """Exact XYZ/flat normals/winding/source identity under the same opaque material."""
    used = source["positions"][source["indices"]].reshape(-1, 3)
    words, remap = np.unique(used.view(np.uint32), axis=0, return_inverse=True)
    normals, normal_index = np.unique(
        source["normal"][source["indices"][:, 0]], return_inverse=True
    )
    ids = sorted(records, key=lambda r: r["index"])
    identity = np.array(
        [(r["index"], r["kind"], r["id"]) for r in ids],
        dtype=[("ordinal", "<u4"), ("kind", "u1"), ("id", "<u8")],
    )
    arrays = {
        "positions": words.view(np.float32),
        "indices": remap.reshape(source["indices"].shape).astype(
            np.uint16 if len(words) <= 65536 else np.uint32
        ),
        "normals": normals,
        "face_normal": normal_index.astype(
            np.uint16 if len(normals) <= 65536 else np.uint32
        ),
        "sources": identity,
        "face_source": np.searchsorted(
            identity["ordinal"], source["binding"][:, 1]
        ).astype(np.uint16 if len(ids) <= 65536 else np.uint32),
    }
    np.testing.assert_array_equal(
        arrays["positions"][arrays["indices"]], source["positions"][source["indices"]]
    )
    np.testing.assert_array_equal(
        normals[arrays["face_normal"]], source["normal"][source["indices"][:, 0]]
    )
    np.testing.assert_array_equal(
        identity["ordinal"][arrays["face_source"]], source["binding"][:, 1]
    )
    np.savez_compressed(path, **arrays)
    return {
        "arrays": {k: a.nbytes for k, a in arrays.items()},
        "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
        "appearance_array_bytes": sum(
            a.nbytes for k, a in arrays.items() if k != "face_source"
        ),
        "container_bytes": path.stat().st_size,
    }


def geometry_controls(source, records, observer_box, output):
    full = geometry_control(source, records, output / "geometry-control.npz")
    support = select_support(
        source["positions"], source["indices"], observer_box, max_blockers=0
    )
    selected = dict(source)
    for key in ("indices", "binding"):
        selected[key] = source[key][support.keep]
    front = geometry_control(selected, records, output / "backface-control.npz")
    return {
        "full": full,
        "backface_only": front,
        "selection_work": support.work,
        "scope": "appearance bytes omit per-face source IDs, which the measure also cannot return; global original identity table retained. Attributed bytes include that stronger capability. No UV, material codes or native packing claim",
    }


def references(source, view, box, prepared=None):
    start = time.perf_counter()
    grid = pixel_grid(source, view, box)
    fragments = list(grid.fragments())
    coarse = quadrature(fragments, view, box)
    fine = quadrature(fragments, view, box, refined=True)
    report = {
        "visibility_and_quadrature_s": time.perf_counter() - start,
        "work": grid.work,
        "quadrature_samples": [len(coarse["rows"]), len(fine["rows"])],
        "observed_sources": np.unique(
            source["binding"][coarse["faces"][coarse["weights"] > 1e-12], 1]
        ).tolist(),
    }
    if prepared is not None:
        eligible = np.flatnonzero(coarse["weights"] > 1e-8)
        indices = eligible[
            np.linspace(0, len(eligible) - 1, min(256, len(eligible))).astype(int)
        ]
        rays, faces = coarse["directions"][indices], coarse["faces"][indices]
        depth, actual = intersect_source(
            source, np.broadcast_to(view[0], rays.shape), rays, prepared=prepared
        )
        expected = 1 / (
            np.column_stack(
                (
                    (rays @ view[1].T)[:, :2] * [view[2], -view[2]]
                    + [view[3] / 2, view[4] / 2]
                    - box[0],
                    np.ones(len(rays)),
                )
            )
            * grid.planes[faces]
        ).sum(1)
        # Camera-space rays have forward Z=1. Triangle plane stores reciprocal Z.
        error = abs(depth - expected)
        normal_disagreement = (
            source["normal"][source["indices"][faces, 0]]
            != source["normal"][source["indices"][np.maximum(actual, 0), 0]]
        )
        report["independent_rtree"] = {
            "samples": len(rays),
            "face_disagreements": int((faces != actual).sum()),
            "normal_disagreements": int(normal_disagreement.sum()),
            "max_depth_error_m": float(error.max(initial=0)),
        }
        if (
            (actual < 0).any()
            or error.max(initial=0) > 1e-6
            or normal_disagreement.any()
        ):
            raise ValueError(
                "analytic visible fragments disagree with independent original rays"
            )
    return grid.shape, coarse, fine, report


def compare(
    source,
    product,
    view,
    box,
    programs,
    light,
    roughness,
    energy,
    *,
    check_sampling=False,
):
    shape, coarse, fine, _ = programs
    reference, alpha = evaluate(fine, source, shape, light, roughness, energy)
    reference_coarse, _ = evaluate(coarse, source, shape, light, roughness, energy)
    origins, directions = image_rays(view, box, *shape, 4)
    start = time.perf_counter()
    values = query(product, origins, directions, light, roughness, energy)
    query_s = time.perf_counter() - start
    mixture, mean, predicted = (
        average_samples(values[key], *shape, 4)
        for key in ("rgb", "mean_rgb", "coverage")
    )
    report = {
        "reference_quadrature_delta": metrics(reference, reference_coarse),
        "reference_coverage_px": float(alpha.sum()),
        "coverage_l1_px": float(abs(alpha - predicted).sum()),
        "coverage_max_abs": float(abs(alpha - predicted).max(initial=0)),
        "mixture": metrics(reference, mixture),
        "mean_toksvig": metrics(reference, mean),
        "query_s": query_s,
        "undefined_mean": values["undefined_mean"],
    }
    if check_sampling:
        origins, directions = image_rays(view, box, *shape, 8)
        more = query(product, origins, directions, light, roughness, energy)
        report["candidate_quadrature_delta"] = metrics(
            mixture, average_samples(more["rgb"], *shape, 8)
        )
        report["candidate_coverage_quadrature_l1_px"] = float(
            abs(predicted - average_samples(more["coverage"], *shape, 8)).sum()
        )
    images = {
        "fine": reference,
        "mixture": mixture,
        "mean_toksvig": mean,
        "original_coverage": alpha,
        "candidate_coverage": predicted,
    }
    return report, images
