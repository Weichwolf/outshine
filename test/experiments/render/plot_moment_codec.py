"""Plot numeric channel/PBR captures, not a native textured-atlas rendering."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("system-temp image required")
    d = json.loads(args.input.read_text())
    figure, axes = plt.subplots(2, 2, figsize=(12, 7), layout="constrained")
    names = {
        "float32": "unencoded",
        "unorm8": "UNORM8",
        "unorm16_rg": "UNORM16 R/G",
        "fp16_moment_a8_control": "FP16 M + A8 control",
        "fp16_rgba": "FP16 RGBA",
    }
    for column, roughness in enumerate((0.12, 0.2)):
        cases = sorted(
            (
                c
                for c in d["cases"]
                if c["pattern"] == "constant"
                and c["roughness"] == roughness
                and c["coverage"] > 0
                and c["lod"] == 0
            ),
            key=lambda c: c["coverage"],
        )
        for mode in d["modes"]:
            for row, field in enumerate(
                ("decoded_roughness", "max_contribution_rgb_error_finite_only")
            ):
                axes[row, column].plot(
                    [c["coverage"] for c in cases],
                    [
                        float("nan")
                        if row == 0 and c[mode]["lost_opaque_coverage"]
                        else c[mode][field]
                        for c in cases
                    ],
                    marker=".",
                    label=names[mode],
                )
        for row in range(2):
            axes[row, column].set(
                xscale="log", xlabel="source opaque coverage", title=f"r={roughness}"
            )
            axes[row, column].grid(alpha=0.2)
        axes[0, column].set_ylabel("decoded perceptual roughness")
        axes[0, column].text(
            0.02,
            0.03,
            "A8=0: conditional r undefined",
            transform=axes[0, column].transAxes,
            fontsize=8,
        )
        axes[1, column].set_ylabel("max linear RGB opaque-contribution error")
        axes[1, column].set_yscale("symlog", linthresh=1e-4)
        axes[1, column].set_ylim(bottom=0)
    axes[0, 0].legend(fontsize=8)
    failures = d["brdf_status"]["nonfinite_native_responses"]
    figure.suptitle(
        "Numeric channel reconstruction + production GGX; hardware texture filtering unproved\n"
        f"{failures} nonfinite small-r reference responses retained separately (#829)"
    )
    figure.savefig(args.output, dpi=130)
    plt.close(figure)


if __name__ == "__main__":
    main()
