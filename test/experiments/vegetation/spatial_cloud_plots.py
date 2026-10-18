"""Same-source coverage and positive-normal diagnostics from finished study arrays."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from normal_lobes import evaluate


def plot(controls, fields, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private plot must be under /tmp")
    with np.load(controls / "host-17/coverage.npz", allow_pickle=False) as z:
        coverage = dict(z)
    with np.load(fields / "host-17-view-0.npz", allow_pickle=False) as z:
        normals = dict(z)
    lights = np.asarray(
        json.loads((fields / "report.json").read_text())["query_lights"]
    )
    mean = normals["moments4"].sum(axis=1, keepdims=True)
    measures = (
        normals["exact_probe_response"],
        evaluate(mean[..., 1:], lights),
        evaluate(normals["moments4"][..., 1:], lights),
        evaluate(normals["moments24"][..., 1:], lights),
    )
    worst = int(np.argmax(np.max(abs(measures[0] - measures[1]), axis=1)))
    fig, axes = plt.subplots(2, 4, figsize=(16, 9), constrained_layout=True)
    titles = (
        "Exact coarse host",
        "Spatial planes",
        "16 of 64 angular views",
        "64 angular views",
    )
    values = (
        coverage["spatial-0-reference"],
        coverage["spatial-0-reduced"],
        coverage["views16_subset-0-reduced"],
        coverage["views64-0-reduced"],
    )
    for ax, value, title in zip(axes[0], values, titles, strict=True):
        ax.imshow(
            value,
            origin="lower",
            vmin=0,
            vmax=1,
            cmap="Greens",
            interpolation="nearest",
        )
        ax.set_title(title)
        ax.set_xlabel("Shared 8 x 8 footprint; independent direction 0")
    for ax, value, title in zip(
        axes[1],
        measures,
        (
            "Finite exact cosine sum",
            "One raw first moment",
            "4 positive bins",
            "24 positive bins",
        ),
        strict=True,
    ):
        ax.imshow(
            value[worst].reshape(16, 16),
            origin="lower",
            vmin=0,
            vmax=1,
            cmap="magma",
            interpolation="nearest",
        )
        error = float(abs(value[worst] - measures[0][worst]).max())
        ax.set_title(f"{title}; max error {error:.3f}")
        ax.set_xlabel(f"Host17, chart0, independent unit light {worst}")
    for ax in axes.flat:
        ax.set_xticks([])
        ax.set_yticks([])
    fig.suptitle(
        "Same saved host: coverage and unlit normal measure are separate tests\n"
        "Rows use different directions/footprints; scalar cosine response is not RGB/PBR or group R/T",
        fontsize=14,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=160)
    plt.close(fig)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("controls", "fields", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    plot(a.controls, a.fields, a.output)
