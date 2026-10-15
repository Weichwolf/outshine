"""Plot the explicit clear-gas diagnostic report; requires Matplotlib, not a runtime render."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root, output = Path(__file__).resolve().parents[3], args.output.resolve()
    if output == root or root in output.parents:
        raise ValueError("private comparison figures belong outside the repository")
    report = json.loads(args.report.read_text())
    authored = {}
    cases = [
        case
        for case in report["cases"]
        if case["utc"] == authored.setdefault(case["name"].split("/")[0], case["utc"])
    ]
    figure, axes = plt.subplots(3, 2, figsize=(16, 9), layout="constrained")
    weights = np.array([0.2126, 0.7152, 0.0722])
    for axis, case in zip(axes.flat, cases, strict=True):
        rows = [row for row in report["results"] if row["case"] == case["name"]]
        distance = np.array([row["distance_km"] for row in rows])
        reference = np.array([row["reference_rgb_per_sr"] for row in rows]) @ weights
        axis.loglog(distance, reference, "k-", label="finite reference")
        for name, style, colour in [
            ("sky_times_loss", "--", "#9a456a"),
            ("midpoint_16", ":", "#2d768d"),
            ("gauss_8", "-.", "#70923e"),
        ]:
            radiance = np.array(
                [row["candidates"][name]["radiance_rgb_per_sr"] for row in rows]
            )
            axis.loglog(distance, radiance @ weights, style, color=colour, label=name)
        axis.set(
            title=f"{case['name'].split('/')[0]}: look {case['elevation_deg']:g}°, "
            f"sun {case['sun_elevation_deg']:.1f}°",
            xlabel="view segment [km]",
            ylabel="Y / solar illuminance [sr⁻¹]",
        )
        axis.grid(True, alpha=0.25)
    axes.flat[0].legend(fontsize=9)
    figure.suptitle(
        "Clear-gas single scattering: authored Place camera inputs, no runtime proof",
        fontsize=16,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=200)
    plt.close(figure)
    print(output)


if __name__ == "__main__":
    main()
