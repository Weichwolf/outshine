"""Diffuse angular mixtures, pigment correlation and the view-hemisphere failure."""

import json
from importlib.resources import files
from pathlib import Path

import matplotlib
import numpy as np
from outshine_ref.vegetation import LeafOpticsTable
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response

matplotlib.use("Agg")
from matplotlib import pyplot as plt

OUTPUT = Path("build/experiments/vegetation/normal-mixture")


def response(optical, normals, view, light):
    return leaf_diffuse_response(optical * 0.5, normals, view, light).sum(axis=0)


def pair(angle):
    x, z = np.sin(angle), np.cos(angle)
    return np.array(((x, 0, z), (-x, 0, z)))


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    table = LeafOpticsTable.from_recipe(
        json.loads(
            (
                files("outshine_ref.vegetation") / "data/leaf-optics-table-1.json"
            ).read_text()
        )
    )
    optical = table.sample(np.array(((45, 0.2), (45, 0.2))))
    cases = []
    for degrees in (15, 45, 75):
        normals = pair(np.deg2rad(degrees))
        for view in ((0, 0, 1), (0, 0, -1)):
            direct = response(optical, normals, view, (1, 0, 0))
            expected = (
                (optical[0, :3] + optical[0, 3:])
                * np.sin(np.deg2rad(degrees))
                / (2 * np.pi)
            )
            mean = leaf_diffuse_response(
                optical.mean(axis=0)[None], (0, 0, 1), view, (1, 0, 0)
            )[0]
            np.testing.assert_allclose(direct, expected, atol=1e-16, rtol=0)
            np.testing.assert_array_equal(mean, np.zeros(3))
            cases.append(
                {
                    "degrees": degrees,
                    "view": view,
                    "direct_rgb": direct.tolist(),
                    "mean_rgb": mean.tolist(),
                    "independent_rgb": expected.tolist(),
                }
            )
    pigments = table.sample(np.array(((45, 0.2), (8, 8))))
    correlated = response(pigments, pair(np.pi / 4), (0, 0, 1), (1, 0, 0))
    expected = (pigments[0, :3] + pigments[1, 3:]) * np.sin(np.pi / 4) / (2 * np.pi)
    np.testing.assert_allclose(correlated, expected, atol=1e-16, rtol=0)
    uncorrelated = response(
        np.broadcast_to(pigments.mean(axis=0), pigments.shape),
        pair(np.pi / 4),
        (0, 0, 1),
        (1, 0, 0),
    )
    assert np.max(abs(correlated - uncorrelated)) > 1e-3
    epsilon = 1e-4
    crossing = response(optical, pair(epsilon), (1, 0, 0), (0, 0, 1))
    mean = leaf_diffuse_response(
        optical.mean(axis=0)[None], (0, 0, 1), (1, 0, 0), (0, 0, 1)
    )[0]
    chord = np.linalg.norm(pair(epsilon) - (0, 0, 1), axis=1)
    conditional_bound = (optical[:, :3] + optical[:, 3:]).T @ chord / (2 * np.pi)
    assert np.max(abs(crossing - mean)) > np.max(conditional_bound)
    report = {
        "table_optical": optical.tolist(),
        "same_pigment_cases": cases,
        "correlated_rgb": correlated.tolist(),
        "uncorrelated_rgb": uncorrelated.tolist(),
        "view_crossing": {
            "epsilon_rad": epsilon,
            "direct_rgb": crossing.tolist(),
            "mean_rgb": mean.tolist(),
            "invalid_same_hemisphere_bound_rgb": conditional_bound.tolist(),
        },
        "scope": "exact discrete diffuse mixture; no distribution compression, specular, masking, occlusion or projected-coverage proof",
    }
    (OUTPUT / "reference.json").write_text(json.dumps(report, indent=2) + "\n")
    angles = np.linspace(-np.pi, np.pi, 721)
    lights = np.column_stack((np.sin(angles), np.zeros_like(angles), np.cos(angles)))
    fig, ax = plt.subplots(figsize=(12, 6.75))
    for degrees in (15, 45, 75):
        colour = [
            response(optical, pair(np.deg2rad(degrees)), (0, 0, 1), l)[1]
            for l in lights
        ]
        ax.plot(np.rad2deg(angles), colour, label=f"Two normals ±{degrees}°")
    mean = [
        leaf_diffuse_response(optical[:1], (0, 0, 1), (0, 0, 1), l)[0, 1]
        for l in lights
    ]
    ax.plot(np.rad2deg(angles), mean, "k--", label="Normalized mean")
    ax.set(
        xlabel="Light angle in XZ plane [degrees]",
        ylabel="Linear green diffuse response [1/sr]",
        title="Diffuse R/T response retains angular modes",
        xlim=(-180, 180),
    )
    ax.legend()
    fig.tight_layout()
    fig.savefig(OUTPUT / "angular-response.png", dpi=160)
    plt.close(fig)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
