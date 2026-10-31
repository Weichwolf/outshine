"""Actual whole-recipe parameter charts; Lambert/normal diagnostics, no PBR."""

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from experiment_output import create_output


def plot_case(root, ordinal, output):
    methods = ("position-only", "adaptive", "uniform")
    reports = []
    plans = []
    for method in methods:
        directory = root / f"{ordinal}-{method}"
        report = json.loads((directory / "report.json").read_text())
        if report["ordinal"] != ordinal or report["method"] != method:
            raise ValueError("Whole original recipe and declared method required")
        reports.append(report)
        if report["accepted"]:
            with np.load(directory / "plan.npz", allow_pickle=False) as saved:
                plans.append({key: saved[key] for key in saved.files})
        else:
            plans.append(None)
    available = next((p for p in plans if p is not None), None)
    if available is None:
        raise ValueError("At least one complete diagnostic required")
    for plan in plans:
        if plan is not None and any(
            not np.array_equal(plan[key], available[key])
            for key in (
                "diagnostic_stations_m",
                "diagnostic_lateral_m",
                "analytic_normals_enh",
            )
        ):
            raise ValueError("Identical whole-recipe diagnostic domain required")
    s, u = available["diagnostic_stations_m"], available["diagnostic_lateral_m"]
    maximum = max(
        float(np.rad2deg(p["observed_normal_error_rad"]).max())
        for p in plans
        if p is not None
    )
    figure, axes = plt.subplots(
        2, 4, figsize=(15, 7), layout="constrained", sharex=True, sharey=True
    )
    axes[0, 0].pcolormesh(
        u,
        s,
        available["analytic_lambert"],
        shading="nearest",
        cmap="gray",
        vmin=0,
        vmax=1,
    )
    axes[0, 0].set_title("Analytic reference / Lambert")
    truth = axes[1, 0].pcolormesh(
        u,
        s,
        available["analytic_normals_enh"][..., 2],
        shading="nearest",
        cmap="viridis",
        vmin=0,
        vmax=1,
    )
    axes[1, 0].set_title("Analytic reference / normal up")
    figure.colorbar(truth, ax=axes[1, 0], label="Normal up")
    error_image = None
    for column, (method, report, plan) in enumerate(
        zip(methods, reports, plans, strict=True), 1
    ):
        if plan is None:
            for panel in axes[:, column]:
                panel.text(
                    0.5,
                    0.5,
                    "Retain original\n" + report["reason"],
                    transform=panel.transAxes,
                    ha="center",
                    va="center",
                    wrap=True,
                )
            axes[0, column].set_title(method)
            continue
        axes[0, column].pcolormesh(
            u,
            s,
            plan["interpolated_lambert"],
            shading="nearest",
            cmap="gray",
            vmin=0,
            vmax=1,
        )
        axes[0, column].set_title(f"{method}: {report['triangles']} triangles")
        error = np.rad2deg(plan["observed_normal_error_rad"])
        error_image = axes[1, column].pcolormesh(
            u, s, error, shading="nearest", cmap="magma", vmin=0, vmax=maximum
        )
        axes[1, column].set_title(f"Observed maximum {error.max():.4g} degrees")
    if error_image is not None:
        figure.colorbar(
            error_image, ax=axes[1, 1:], label="Sampled normal error [degrees]"
        )
    for panel in axes.flat:
        panel.set_xlabel("Lateral coordinate [m]")
        panel.set_ylabel("Original station [m]")
    figure.suptitle(
        f"Complete original {ordinal}; parameter-chart diagnostics, no physical/PBR/World qualification"
    )
    figure.savefig(output / f"{ordinal}.png", dpi=130)
    plt.close(figure)
    return {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in root.glob(f"{ordinal}-*/*")
        if path.is_file()
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--ordinal", type=int, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    create_output(args.output)
    inputs = {
        str(ordinal): plot_case(args.input, ordinal, args.output)
        for ordinal in args.ordinal
    }
    (args.output / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
