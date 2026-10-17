"""Spatially filtered geometry coefficients for the existing finite D+KD model."""

import argparse
import hashlib
import json
import resource
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from canopy_response import footprint_samples, prepare_response
from exchange_light import basis_optics
from exchange_operator import mix_sides
from grass_light_experiment import light_states


def mean_cells(values, width):
    height, size = values.shape[:2]
    if height != size or size % width:
        raise ValueError(
            "square samples divisible by the requested cell width required"
        )
    step = size // width
    return values.reshape(width, step, width, step, *values.shape[2:]).mean((1, 3))


def field_coefficients(matrix, fields, samples, shape, leaf_nodes, width):
    leaf = samples < 2 * leaf_nodes
    result = []
    for source in fields:
        incoming = matrix @ source
        own = incoming[samples]
        other = incoming[samples ^ 1]
        quadratic = np.stack(
            (own * leaf[:, None], other * leaf[:, None], own * (~leaf)[:, None]), axis=1
        ).reshape(-1, 9)
        coefficients = np.concatenate((source[samples], quadratic), axis=1)
        result.append(mean_cells(coefficients.reshape(*shape, 12), width))
    return np.stack(result, axis=2)


def query_states(directions):
    control = json.loads(
        (Path(__file__).parents[1] / "vegetation/grass_light_control.json").read_text()
    )
    states = light_states(control)
    states["back-no-transmission"] = json.loads(json.dumps(states["back"]))
    states["back-no-transmission"]["leaf_rt"][3:] = [0, 0, 0]
    states["ground-only"] = json.loads(json.dumps(states["front"]))
    states["ground-only"]["leaf_rt"] = [0] * 6
    states["ground-only"]["ground_albedo"] = [1, 1, 1]
    materials, energies, names = [], [], []
    for name, state in states.items():
        materials.append(
            np.array(state["leaf_rt"] + state["ground_albedo"]).reshape(3, 3)
        )
        energy = np.zeros((len(directions), 3))
        for lamp in state["lights"]:
            energy[directions.index(lamp["direction"])] += lamp["irradiance_rgb"]
        energies.append(energy)
        names.append(name)
    rng = np.random.default_rng(451)
    for trial in range(16):
        material = rng.uniform(0, 1, (3, 3))
        material[:2] /= max(1, material[0].max() + material[1].max())
        if trial == 0:
            material[:] = 0
        materials.append(material)
        energies.append(rng.uniform(0, 2, (len(directions), 3)))
        names.append(f"control-{trial}")
    return names, np.array(materials), np.array(energies)


def evaluate(coefficients, material, energy):
    linear, quadratic = (
        coefficients[..., :3],
        coefficients[..., 3:].reshape(*coefficients.shape[:-1], 3, 3),
    )
    direct = np.einsum("hbli,ic,lc->hbc", linear, material, energy)
    first = np.einsum("hblij,ic,jc,lc->hbc", quadratic, material, material, energy)
    return np.concatenate((direct, first), axis=-1)


def compare(matrix, basis, fields, samples, shape, coefficients, materials, energies):
    references, errors, means = [], [], []
    for material, energy in zip(materials, energies, strict=True):
        outgoing = np.einsum("lni,ic,lc->nc", fields, material, energy)
        state = {"leaf_rt": material[:2].ravel(), "ground_albedo": material[2]}
        first = mix_sides(matrix @ outgoing, basis_optics(basis, state))
        fine = np.concatenate((outgoing[samples], first[samples]), axis=1).reshape(
            *shape, 6
        )
        reference = mean_cells(fine, len(coefficients))
        actual = evaluate(coefficients, material, energy)
        errors.append(float(np.max(abs(reference - actual))))
        means.append(float(np.max(abs(reference.mean((0, 1)) - actual.mean((0, 1))))))
        references.append(reference)
    assert max(errors) <= 1e-12
    return np.array(references), max(errors), max(means)


def run(inputs, output, width):
    output = output.resolve()
    if (
        output.exists()
        or not output.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or output.is_relative_to(Path(__file__).resolve().parents[3])
        or width not in (16, 32, 64)
    ):
        raise ValueError("fresh system-temp output and width16/32/64 required")
    began = time.perf_counter()
    basis, matrix, fields, directions, links = prepare_response(inputs)
    samples, shape = footprint_samples(inputs, basis)
    coefficients = field_coefficients(
        matrix, fields, samples, shape, basis["leaf_nodes"], width
    )
    preparation = time.perf_counter() - began
    names, materials, energies = query_states(directions)
    references, error, mean_error = compare(
        matrix, basis, fields, samples, shape, coefficients, materials, energies
    )
    packed = coefficients.astype("<f4")
    packed_error = max(
        float(np.max(abs(evaluate(packed, m, e) - r)))
        for m, e, r in zip(materials, energies, references, strict=True)
    )
    assert packed_error <= 1e-6
    levels = [packed]
    while len(levels[-1]) > 1:
        levels.append(
            mean_cells(levels[-1].astype(float), len(levels[-1]) // 2).astype("<f4")
        )
    mip_error = max(
        float(
            np.max(
                abs(
                    level.astype(float).mean((0, 1)) - packed.astype(float).mean((0, 1))
                )
            )
        )
        for level in levels
    )
    assert mip_error < 1e-6
    output.mkdir(parents=True)
    np.savez(
        output / "coefficients.npz", **{f"level_{i}": a for i, a in enumerate(levels)}
    )
    np.savez(
        output / "frame-diagnostics.npz",
        materials=materials,
        energies=energies,
        reference=references,
    )
    report = {
        "width": width,
        "states": names,
        "directions": directions,
        "stand_identity": basis["stand_identity"],
        "geometry_sha256": basis["geometry_sha256"],
        "contact_sha256": hashlib.sha256(
            (inputs / "east.npz").read_bytes()
        ).hexdigest(),
        "f64_max_abs": error,
        "f64_mean_error": mean_error,
        "f32_coeff_max_abs": packed_error,
        "mip_moment_error": mip_error,
        "payload_bytes": packed.nbytes,
        "mip_payload_bytes": sum(a.nbytes for a in levels),
        "preparation_s": preparation,
        "wall_s": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "source_fields_bytes": fields.nbytes,
        "links_bytes": links.nbytes,
        "scope": "Finite fixed-view six-beam nodal D+KD field; no angular, native world or continuous-image admission",
    }
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=64)
    args = parser.parse_args()
    run(args.inputs, args.output, args.width)
