"""Plot the normal-incidence subset; retain nonfinite/zero markers explicitly."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    rows = np.load(args.capture / "fields.npy")
    values = np.fromfile(args.capture / "native/native.f32", dtype="<f4").reshape(-1, 8)
    expected = np.load(args.capture / "exact-r4-reference.npy")
    mask = (
        (rows[:, 0] > 0)
        & (rows[:, 1] == 1)
        & (rows[:, 2] == 1)
        & (rows[:, 5] == np.float32(0.04))
        & (rows[:, 7] == 0)
    )
    r, actual, reference = rows[mask, 0], values[mask, 4], expected[mask, 3]
    figure, axes = plt.subplots(1, 2, figsize=(12, 4.5), layout="constrained")
    axes[0].loglog(r, reference, "o-", label="Decimal, exact represented r⁴")
    finite_positive = np.isfinite(actual) & (actual > 0)
    axes[0].loglog(
        r[finite_positive],
        actual[finite_positive],
        "x",
        label="Product GPU, finite positive",
    )
    top = axes[0].get_ylim()[1]
    axes[0].scatter(
        r[~np.isfinite(actual)],
        np.full((~np.isfinite(actual)).sum(), top),
        marker="v",
        color="red",
        label="Nonfinite (marker at top)",
    )
    axes[0].scatter(
        r[actual == 0],
        np.full((actual == 0).sum(), 1e-3),
        marker="^",
        color="orange",
        label="Zero (marker at 10⁻³)",
    )
    axes[0].set(
        xlabel="Perceptual roughness r",
        ylabel="Linear specular response",
        title="N=L=V, F0=0.04; original energy scale",
    )
    axes[1].semilogx(
        r[finite_positive],
        (actual[finite_positive] / reference[finite_positive] - 1) * 100,
        "o-",
    )
    axes[1].axhline(0, color="black", linewidth=0.5)
    axes[1].set(
        xlabel="Perceptual roughness r",
        ylabel="Error (%)",
        title="Finite positive responses only; other cases at left",
    )
    for axis in axes:
        axis.grid(alpha=0.25)
    axes[0].legend(fontsize=8)
    figure.savefig(args.output, dpi=150)
    plt.close(figure)


if __name__ == "__main__":
    main()
