"""Inspect numeric port results against their bound DEM; no runtime-render proof."""

import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np
from canonical_port_native_inputs import FIXTURE, load

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--replay", type=Path, required=True)
    parser.add_argument("--inputs", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh explicit diagnostic output required")
    replay = json.loads(args.replay.read_text())
    source, terrain, _binding, manifest = load(args.inputs)
    if replay["native_raw_sha256"] != manifest["native_raw_sha256"]:
        parser.error("replay and source inputs differ")
    values = [value for value in replay["values"] if value is not None]
    if not values:
        parser.error("no numeric ports to inspect")
    nodes = {node.identifier: node.longitude_latitude for node in source.nodes}
    selected = {
        tuple(edge)
        for row in replay["report"]["components"]
        for edge in row["selected"]
    }
    positions = np.array([value["anchor_longitude_latitude"] for value in values])
    figure, axes = plt.subplots(1, 3, figsize=(15, 4.5), layout="constrained")
    for span in source.spans:
        if span.identifier in selected:
            xy = np.array([nodes[node] for node in span.nodes])
            axes[0].plot(*xy.T, color="0.55")
    axes[0].scatter(*positions.T, c=np.arange(len(values)), cmap="viridis")
    for i, xy in enumerate(positions):
        axes[0].annotate(str(i), xy, xytext=(4, 4), textcoords="offset points")
    axes[0].set(
        xlabel="longitude (deg)",
        ylabel="latitude (deg)",
        title="Admitted whole source chains and native-grid ports",
    )
    axes[0].set_aspect(1 / np.cos(np.deg2rad(positions[:, 1].mean())))
    index = np.arange(len(values))
    axes[1].plot(index, terrain(positions), "o", label="bound DEM")
    axes[1].plot(
        index, [v["height_m"] for v in values], "+", label="canonical model height"
    )
    axes[1].set(
        xlabel="port index (separate geographic anchors)",
        ylabel="explicit model height (m)",
        title="DEM priors and source-derived port heights",
    )
    axes[1].legend()
    axes[2].plot(
        index,
        [v["longitudinal_grade"] for v in values],
        "o",
        label="longitudinal grade",
    )
    axes[2].plot(
        index,
        [-np.tan(v["crossfall_rad"]) for v in values],
        "+",
        label="left cross grade",
    )
    axes[2].set(
        xlabel="port index", ylabel="rise / run", title="Slopes in each port's own ENU"
    )
    axes[2].legend()
    figure.suptitle(
        "Ground-only Python reference: no bridge, world, native mesh or device admission"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, dpi=140)
    plt.close(figure)
    print(args.output)


if __name__ == "__main__":
    main()
