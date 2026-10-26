"""Compare two canonical replay recipes on identical bound inputs and cuts."""

import argparse
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--first", type=Path, required=True)
    parser.add_argument("--second", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh explicit diagnostic output required")
    first, second = [json.loads(path.read_text()) for path in (args.first, args.second)]
    if first["demands"] != second["demands"] or any(
        first["report"][key] != second["report"][key]
        for key in ("source_binding", "terrain_binding")
    ):
        parser.error("identical bound inputs and ordered requests required")
    quality = first["report"]["procedure"]["maximum_prior_change_m"]
    if quality != second["report"]["procedure"]["maximum_prior_change_m"]:
        parser.error("identical prior quality requirement required")
    if any(value is None for report in (first, second) for value in report["values"]):
        parser.error("numeric comparisons require resolved values")
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 3, figsize=(15, 4.5), layout="constrained")
    heights = []
    for report, marker in ((first, "o"), (second, "+")):
        method = report["report"]["procedure"]["requested_method"]
        values = report["values"]
        height = np.array([value["height_m"] for value in values])
        heights.append(height)
        axes[0].plot(height, marker, label=method)
        axes[2].plot(
            [v["longitudinal_grade"] for v in values],
            marker + "-",
            label=method + " longitudinal",
        )
        axes[2].plot(
            [-np.tan(v["crossfall_rad"]) for v in values],
            marker + ":",
            label=method + " left cross",
        )
    axes[0].set(
        ylabel="own-ENU model geodetic height (m)", title="Canonical port heights"
    )
    axes[1].plot(heights[0] - heights[1], "o")
    axes[1].set(
        ylabel="first minus second (m)",
        title="Recipe difference; no numerical equivalence",
    )
    axes[2].set(ylabel="rise / run", title="Physical port gradients")
    for axis in axes:
        axis.set(xlabel="same native geographic cut index")
    axes[0].legend()
    axes[2].legend()
    figure.suptitle(
        f"Identical bound sources and DEM; {quality:g} m prior requirement, no native/world/device admission"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, dpi=140)
    plt.close(figure)
    print(args.output)


if __name__ == "__main__":
    main()
