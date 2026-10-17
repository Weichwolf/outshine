"""Fixed-footprint D+KD polynomial; no angular interpolation or native admission."""

import argparse
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from exchange_basis import prepare_stand, source_nodes
from exchange_light import basis_optics, direct_nodes
from exchange_operator import build_links, mix_sides, prepare_csr
from grass_light_experiment import light_states
from grass_projection_stand import DIRECTIONS, LOWER, UPPER


def footprint_weights(inputs, basis):
    with np.load(inputs / "east.npz", allow_pickle=False) as product:
        face, height, normal = (product[k] for k in ("face", "height", "normal"))
    size = len(face)
    y, x = np.indices(face.shape)
    q = LOWER + np.stack((x + 0.5, y + 0.5), -1) / size * (UPPER - LOWER)
    view = DIRECTIONS["east"]
    points = np.concatenate((q, np.zeros((*face.shape, 1))), -1)
    points += height[..., None] * view / view[2]
    ids = np.where(face >= 0, face, basis["leaf_nodes"])
    nodes = source_nodes(ids.ravel(), points.reshape(-1, 3), basis)
    side = (normal @ view < 0).ravel() & (face.ravel() >= 0)
    return np.bincount(2 * nodes + side, minlength=2 * len(basis["points"])) / face.size


def coefficients(matrix, direct, weights, leaf_nodes):
    receiver = np.zeros((3, len(weights)))
    receiver[0, : 2 * leaf_nodes] = weights[: 2 * leaf_nodes]
    receiver[1, : 2 * leaf_nodes] = (
        weights[: 2 * leaf_nodes].reshape(-1, 2)[:, ::-1].ravel()
    )
    receiver[2, 2 * leaf_nodes :] = weights[2 * leaf_nodes :]
    return weights @ direct, receiver @ (matrix @ direct)


def evaluate(linear, quadratic, material, energy):
    direct = np.einsum("li,ic->lc", linear, material)
    first = np.einsum("lij,ic,jc->lc", quadratic, material, material)
    return np.sum(energy * (direct + first), axis=0)


def verify(matrix, basis, fields, weights, linear, quadratic):
    rng = np.random.default_rng(451)
    errors, packed_errors, full_seconds, reduced_seconds, cases = [], [], [], [], []
    for trial in range(20):
        material = rng.uniform(0, 1, (3, 3))
        material[:2] /= max(1, material[0].max() + material[1].max())
        if trial == 0:
            material[:] = 0
        energy = rng.uniform(0, 2, (len(fields), 3))
        state = {"leaf_rt": [*material[0], *material[1]], "ground_albedo": material[2]}
        began = time.perf_counter()
        outgoing = np.einsum("lni,ic,lc->nc", fields, material, energy)
        full = weights @ (
            outgoing + mix_sides(matrix @ outgoing, basis_optics(basis, state))
        )
        full_seconds.append(time.perf_counter() - began)
        began = time.perf_counter()
        reduced = evaluate(linear, quadratic, material, energy)
        reduced_seconds.append(time.perf_counter() - began)
        packed = evaluate(
            linear.astype(np.float32), quadratic.astype(np.float32), material, energy
        )
        errors.append(float(np.max(abs(full - reduced))))
        packed_errors.append(float(np.max(abs(full - packed))))
        cases.append(
            {
                "material": material.tolist(),
                "energy": energy.tolist(),
                "full": full.tolist(),
                "reduced": reduced.tolist(),
                "packed": packed.tolist(),
            }
        )
    assert max(errors) <= 1e-12
    assert max(packed_errors) <= 1e-6
    return {
        "trials": 20,
        "cases": cases,
        "f64_max_abs": max(errors),
        "f32_coeff_max_abs": max(packed_errors),
        "full_median_s": float(np.median(full_seconds)),
        "reduced_median_s": float(np.median(reduced_seconds)),
    }


def run(inputs, output):
    output = output.resolve()
    if (
        output.exists()
        or not output.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or output.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        raise ValueError("fresh explicit system-temp output outside checkout required")
    began = time.perf_counter()
    scene, basis = prepare_stand(inputs, 64)
    links = build_links(
        scene,
        basis["points"],
        basis["normals"],
        4,
        lambda i, p: source_nodes(i, p, basis),
    )
    matrix = prepare_csr(links)
    weights = footprint_weights(inputs, basis)
    control = json.loads(
        (Path(__file__).parents[1] / "vegetation/grass_light_control.json").read_text()
    )
    states = light_states(control)
    directions = []
    for state in states.values():
        for lamp in state["lights"]:
            if lamp["direction"] not in directions:
                directions.append(lamp["direction"])
    probes = {
        str(i): {
            "leaf_rt": [0.5, 0, 0, 0, 0.5, 0],
            "ground_albedo": [0, 0, 1],
            "lights": [{"direction": d, "irradiance_rgb": [1, 1, 1]}],
        }
        for i, d in enumerate(directions)
    }
    direct = direct_nodes(scene, basis, probes)
    fields = np.array([direct[str(i)] * [2, 2, 1] for i in range(len(directions))])
    del direct
    pairs = [coefficients(matrix, f, weights, basis["leaf_nodes"]) for f in fields]
    linear = np.array([p[0] for p in pairs])
    quadratic = np.array([p[1] for p in pairs])
    preparation = time.perf_counter() - began
    checks = verify(matrix, basis, fields, weights, linear, quadratic)
    output.mkdir(parents=True)
    np.savez(output / "coefficients.npz", linear=linear, quadratic=quadratic)
    report = {
        "checks": checks,
        "preparation_s": preparation,
        "geometry_sha256": basis["geometry_sha256"],
        "stand_identity": basis["stand_identity"],
        "view": DIRECTIONS["east"].tolist(),
        "directions": directions,
        "f32_payload_bytes": (linear.size + quadratic.size) * 4,
        "links_bytes": links.nbytes,
        "csr_bytes": matrix.data.nbytes + matrix.indices.nbytes + matrix.indptr.nbytes,
        "nodes": len(basis["points"]),
        "weight_sum": float(weights.sum()),
        "scope": "Fixed 1m2 footprint and directions, homogeneous RGB R/T and ground, direct plus one neighbour only; no native, angular, image or world acceptance",
    }
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: v for k, v in report.items() if k != "checks"}
            | {"checks": {k: v for k, v in checks.items() if k != "cases"}}
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
