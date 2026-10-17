"""Fixed actual source/camera cohort; share angular modes, retain per-view fields."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from joint_basis_mips import matrix_for, summary
from joint_basis_reduction import interpolate
from needle_response_study import source_digest
from normal_lobe_study import coarsen
from outshine_ref.vegetation.normal_distribution import evaluate_diffuse
from positive_light_basis import angular_weights


def load_case(path):
    report = json.loads((path / "report.json").read_text())
    if (
        report["angular_coordinates"] != "view"
        or report["knot_count"] != 64
        or not report["normal_moments"]
    ):
        raise ValueError("aligned64 source study with actual normal moments required")
    with np.load(path / "fields.npz") as saved:
        arrays = {key: saved[key] for key in ("knots", "heldout", "views")}
        for name in (
            "joint",
            "unshadowed",
            "coverage",
            "normal_moments",
            "directions",
        ):
            arrays[name] = np.stack([saved[f"{name}{i}"] for i in range(3)])
    expected = {
        "joint": (3, 72, 256),
        "unshadowed": (3, 72, 256),
        "coverage": (3, 256),
        "normal_moments": (3, 256, 4, 4),
        "directions": (3, 72, 3),
    }
    if any(arrays[k].shape != shape for k, shape in expected.items()) or not all(
        np.isfinite(a).all() for a in arrays.values()
    ):
        raise ValueError("same finite bounded source-grid cohort required")
    arrays["identity"] = {
        "path": str(path),
        "capacity": report["capacity"],
        "order": report["order"],
        "source_sha256": report["source_sha256"],
        "fields_sha256": source_digest(path / "fields.npz"),
        "bake_commit": report["commit"],
    }
    return arrays


def level_fields(case):
    pixels = case["coverage"].shape[-1]
    truth = case["joint"][:, 64:].transpose(0, 2, 1).reshape(-1, 8)
    mass = case["coverage"].reshape(-1, 1)
    normal, mean = [], []
    for moments, directions in zip(
        case["normal_moments"], case["directions"], strict=True
    ):
        normal.append(evaluate_diffuse(moments[..., 1:], directions[64:]).T)
        total = moments.sum(axis=1)
        length = np.linalg.norm(total[:, 1:], axis=1)
        vectors = np.divide(
            total[:, 1:] * total[:, :1],
            length[:, None],
            out=np.zeros_like(total[:, 1:]),
            where=length[:, None] > 0,
        )
        mean.append(evaluate_diffuse(vectors[:, None], directions[64:]).T)
    normal, mean = np.concatenate(normal), np.concatenate(mean)
    unshadowed = case["unshadowed"][:, 64:].transpose(0, 2, 1).reshape(-1, 8)
    if np.max(normal - unshadowed) > 2e-12:
        raise AssertionError("four-lobe finite-measure lower bound violated")
    np.testing.assert_allclose(
        case["normal_moments"][..., 0].sum(axis=-1),
        case["coverage"],
        atol=2e-12,
        rtol=0,
    )
    cones = [angular_weights(case["knots"], q) for q in case["heldout"]]
    matrix = matrix_for(case["joint"], 64)
    raw = np.concatenate(
        [matrix[v * pixels : (v + 1) * pixels] @ w for v, w in enumerate(cones)]
    )
    baseline = {
        name: summary(value, truth, mass)
        for name, value in [
            ("angular", raw),
            ("normal4_no_self_shadow", normal),
            ("normalized_mean_no_self_shadow", mean),
            ("exact_unshadowed", unshadowed),
        ]
    }
    return matrix, truth, mass, cones, baseline


def project_modes(training, target, truth, mass, cones):
    _, singular, angular = np.linalg.svd(training, full_matrices=False)
    results, fields = {}, {}
    for requested in (4, 8, 12):
        rank = min(requested, len(singular))
        coefficients = angular[:rank]
        spatial = target @ coefficients.T
        prediction = interpolate(spatial, coefficients, cones, len(mass) // 3)
        results[str(requested)] = summary(prediction, truth, mass) | {
            "actual_rank": rank,
            "spatial_float32_bytes": spatial.size * 4,
            "shared_angular_float32_bytes": coefficients.size * 4,
        }
        fields[requested] = np.clip(prediction, 0, mass)
    return results, fields


def coarsen_case(case, size):
    result = dict(case)
    for name in ("joint", "unshadowed"):
        result[name] = np.stack([coarsen(field.T, size).T for field in case[name]])
    result["coverage"] = np.stack(
        [coarsen(field[:, None], size).ravel() for field in case["coverage"]]
    )
    result["normal_moments"] = np.stack(
        [coarsen(field, size) for field in case["normal_moments"]]
    )
    return result


def run(training_paths, evaluation_path, output):
    if not output.resolve().is_relative_to(Path("/tmp")) or len(training_paths) != 3:
        raise ValueError("three bounded training cases and private/tmp output required")
    training = [load_case(p) for p in training_paths]
    target = load_case(evaluation_path)
    if [c["identity"]["capacity"] for c in training] != [32, 2, 8] or target[
        "identity"
    ]["capacity"] != 32:
        raise ValueError("fixed32/2/8training and32new-camera evaluation required")
    if target["identity"]["source_sha256"] != training[0]["identity"][
        "source_sha256"
    ] or np.array_equal(target["views"], training[0]["views"]):
        raise ValueError(
            "same32source with genuinely different camera directions required"
        )
    if len({str(p.resolve()) for p in [*training_paths, evaluation_path]}) != 4:
        raise ValueError("distinct fixed training/evaluation cases required")
    for case in training:
        np.testing.assert_array_equal(case["knots"], target["knots"])
        np.testing.assert_array_equal(case["heldout"], target["heldout"])
    identities = [case["identity"] for case in training] + [target["identity"]]
    levels, arrays = [], {}
    for size in (16, 8, 4, 2, 1):
        matrix, truth, mass, cones, baseline = level_fields(target)
        rows = [matrix_for(case["joint"], 64) for case in training]
        single, _ = project_modes(rows[0], matrix, truth, mass, cones)
        pooled, fields = project_modes(np.concatenate(rows), matrix, truth, mass, cones)
        levels.append(
            {
                "size": size,
                "baseline": baseline,
                "original32_modes": single,
                "pooled_modes": pooled,
            }
        )
        arrays[f"truth{size}"] = truth
        arrays[f"coverage{size}"] = mass
        for rank, field in fields.items():
            arrays[f"pooled{rank}_{size}"] = field
        if size > 1:
            training = [coarsen_case(case, size) for case in training]
            target = coarsen_case(target, size)
    output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(output / "fields.npz", **arrays)
    (output / "report.json").write_text(
        json.dumps(
            {
                "inputs": identities,
                "levels": levels,
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], text=True
                ).strip(),
                "scope": "new camera angular-mode evaluation; its spatial maps still extracted from its own64light source bake, six independent evaluation lights; no generated unseen view/continuous-source/native admission",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--training", type=Path, nargs=3, required=True)
    parser.add_argument("--evaluation", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.training, args.evaluation, args.output)
