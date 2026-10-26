"""Actual held-out material failure and a synthetic normal-map averaging control."""

import json

import numpy as np
from building_brdf_basis import angular_samples, response_basis
from building_brdf_fit import emitted_error
from building_mass_lighting import shade
from fit_building_brdf import read_fit, read_material


def normal_counter(albedo, weights, palette, tables):
    angle = np.pi / 12
    a = np.array([np.sin(angle), 0, np.cos(angle)])
    b = a * np.array([-1, 1, 1])
    responses = []
    for normal in (a, b, np.array([0, 0, 1.0])):
        value, _ = shade(
            np.array(albedo)[None],
            np.array(weights)[None],
            palette,
            normal[None],
            a[None],
            a,
            tables,
            intensity=1,
            ambient=0,
        )
        responses.append(value[0])
    return {
        "angle_radians": float(angle),
        "normals": [a.tolist(), b.tolist()],
        "light": a.tolist(),
        "outgoing": a.tolist(),
        "equal_texel_response": ((responses[0] + responses[1]) / 2).tolist(),
        "normalized_mean_response": responses[2].tolist(),
        "scope": "Equal-weight synthetic normal-map samples in existing shade model. No macro occlusion, visibility reconstruction or source-roof admission; a noncommuting normal-average control only.",
    }


def counterexamples(source, fit_root, tables):
    receipt = json.loads((fit_root / "1/report.json").read_text())
    material, record = max(
        ((m, r) for r in receipt["nodes"] for m in r["materials"]),
        key=lambda pair: pair[0]["validation_maximum_relative_channel_error"],
    )
    path = source / record["node_file"]
    data = read_material(path)
    group, role = material["group"], material["role"]
    albedo = data["m__albedo"][group, role]
    palette = data["m__roughness_palette"]
    weights = {"full": data["m__roughness_weights"][group, role]}
    for budget in (1, 2):
        fitted = read_fit(
            fit_root / str(budget) / path.name,
            path,
            data["m__roughness_weights"].shape,
            budget,
        )
        weights[str(budget)] = fitted[group, role]
    samples = angular_samples(validation=True)
    basis = response_basis(palette, tables, samples)
    error = emitted_error(weights["full"], weights["1"], albedo, basis)
    row = np.unravel_index(np.argmax(error), error.shape)[0]
    nl, nv, phi = samples[row]
    light = np.array([np.sqrt(1 - nl * nl), 0, nl])
    outgoing = np.array(
        [[np.sqrt(1 - nv * nv) * np.cos(phi), np.sqrt(1 - nv * nv) * np.sin(phi), nv]]
    )
    values = {}
    for name, w in weights.items():
        values[name] = shade(
            albedo[None],
            w[None],
            palette,
            np.array([[0, 0, 1.0]]),
            outgoing,
            light,
            tables,
            intensity=1,
            ambient=0,
        )[0][0].tolist()
    return {
        "material": {
            "node_file": path.name,
            "group": group,
            "role": role,
            "heldout_row": int(row),
            "sample": samples[row].tolist(),
            "albedo": albedo.tolist(),
            "weights": {k: v.tolist() for k, v in weights.items()},
            "direct_rgb": values,
            "scope": "Actual source-material counterexample from the unchanged fixed held-out angular set; unit direct light/zero ambient, same normal. Not a new world camera/light or fitting-grid sweep.",
        },
        "normal": normal_counter(albedo, weights["full"], palette, tables),
    }


def plot_counterexamples(cases, path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(11, 4))
    x = np.arange(3)
    for i, (name, value) in enumerate(cases["material"]["direct_rgb"].items()):
        axes[0].bar(x + (i - 1) * 0.25, value, 0.25, label=name)
    axes[0].set_title("Actual parent material: held-out response")
    for i, (name, key) in enumerate(
        (
            ("two normal samples", "equal_texel_response"),
            ("unit mean normal", "normalized_mean_response"),
        )
    ):
        axes[1].bar(x + (i - 0.5) * 0.3, cases["normal"][key], 0.3, label=name)
    axes[1].set_title("Synthetic normal-map counterexample")
    for axis in axes:
        axis.set_xticks(x, ["R", "G", "B"])
        axis.set_ylabel("linear direct radiance")
        axis.legend()
    figure.tight_layout()
    figure.savefig(path, dpi=160)
    plt.close(figure)
