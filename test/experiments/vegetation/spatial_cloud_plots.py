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
    field_report = json.loads((fields / "report.json").read_text())
    case = max(
        field_report["cases"],
        key=lambda row: row["mean"]["maximum_unit_response_error"],
    )
    node, chart = case["host_node"], case["view"]
    with np.load(fields / f"host-{node}-view-{chart}.npz", allow_pickle=False) as z:
        normals = dict(z)
    lights = np.asarray(field_report["query_lights"])
    mean = normals["moments4"].sum(axis=1, keepdims=True)
    measures = (
        normals["exact_probe_response"],
        evaluate(mean[..., 1:], lights),
        evaluate(normals["moments4"][..., 1:], lights),
        evaluate(normals["moments24"][..., 1:], lights),
    )
    worst = int(np.argmax(np.max(abs(measures[0] - measures[1]), axis=1)))
    fig, axes = plt.subplots(2, 4, figsize=(16, 9))
    fig.subplots_adjust(
        left=0.025, right=0.99, bottom=0.08, top=0.87, hspace=0.35, wspace=0.06
    )
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
        ax.set_title(title, fontsize=11)
        ax.set_xlabel("Host17, query0, shared 8 x 8 footprint", fontsize=9)
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
        ax.set_title(f"{title}\nmax error {error:.3f}", fontsize=11)
        ax.set_xlabel(f"Host{node}, chart{chart}, unit light{worst}", fontsize=9)
    for ax in axes.flat:
        ax.set_xticks([])
        ax.set_yticks([])
    fig.suptitle(
        "Same saved crown: coverage and unlit normal measure are separate tests\n"
        "Top: host17; bottom: worst first-moment error of 12 fields. Scalar cosine response is not group R/T",
        fontsize=13,
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
