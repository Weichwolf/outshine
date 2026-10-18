"""Explicitly magnified native-pixel crops from actual full-resolution renders."""

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib
import numpy as np
from twig_light_plot import srgb

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def panel_grid(groups, output, title):
    fig, axes = plt.subplots(len(groups), 3, figsize=(16, 9), dpi=160, squeeze=False)
    evidence = []
    for row, group in enumerate(groups):
        arrays = []
        for _, path in group:
            with np.load(path, allow_pickle=False) as data:
                arrays.append(data["rgba"])
        covered = np.any(np.stack([a[..., 3] > 0.005 for a in arrays]), axis=0)
        y, x = np.nonzero(covered)
        if not len(x):
            raise ValueError("actual positive-coverage comparison required")
        lo = np.maximum([y.min() - 8, x.min() - 8], 0)
        hi = np.minimum([y.max() + 9, x.max() + 9], covered.shape)
        crop = np.s_[lo[0] : hi[0], lo[1] : hi[1]]
        members = []
        for col, ((label, path), rgba) in enumerate(zip(group, arrays, strict=True)):
            alpha = rgba[..., 3]
            rgb = rgba[..., :3] + 0.18 * (1 - alpha[..., None])
            axes[row, col].imshow(
                np.clip(srgb(rgb[crop]), 0, 1), origin="lower", interpolation="nearest"
            )
            axes[row, col].set_title(
                label
                + f"\nalpha sum {alpha.sum():.3f}; crop {hi[1] - lo[1]}×{hi[0] - lo[0]}px enlarged",
                fontsize=10,
            )
            members.append(
                {
                    "label": label,
                    "source": str(path),
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "alpha_sum": float(alpha.sum(dtype=np.float64)),
                    "premultiplied_linear_rgb_sum": rgba[..., :3]
                    .sum(axis=(0, 1), dtype=np.float64)
                    .tolist(),
                }
            )
        evidence.append(
            {"native_crop_bounds": [lo.tolist(), hi.tolist()], "members": members}
        )
    for axis in axes.flat:
        axis.set_axis_off()
    fig.suptitle(title, fontsize=16)
    fig.text(
        0.5,
        0.02,
        "Actual2560×1440EEVEE frames, linear premultiplied RGB over fixed0.18background. Enlarged diagnostics add no detail; no whole/transport/LOD approval.",
        ha="center",
        fontsize=9,
    )
    fig.tight_layout(rect=(0, 0.04, 1, 0.94))
    fig.savefig(output)
    plt.close(fig)
    output.with_suffix(".json").write_text(json.dumps(evidence, indent=2) + "\n")


def plots(root):
    groups = []
    for label, names in [
        (
            "Full256 / native384px",
            [
                "full-physical-front-384-valid",
                "full-windows-front-384-valid",
                "full-windows-front-384-single",
            ],
        ),
        (
            "Tail35 / native64px",
            [
                "tail-physical-front-64-matched",
                "tail-windows-front-64-matched",
                "tail-windows-front-64-single",
            ],
        ),
        (
            "Full256 / native16px",
            [
                "full-physical-front-16-matched",
                "full-windows-front-16-matched",
                "full-windows-front-16-single",
            ],
        ),
    ]:
        groups.append(
            [
                (f"{label} | {member}", root / name / "linear.npz")
                for member, name in zip(
                    ("explicit needles", "separate role cards", "one shared card"),
                    names,
                    strict=True,
                )
            ]
        )
    panel_grid(
        groups,
        root / "coincident-panels.png",
        "Same saved needle intervals | light=view | unchanged source and coverage",
    )
    linking = [
        [
            (name, root / "linking" / f"{name}.npz")
            for name in ("all", "linked", "clear")
        ],
        [
            ("explicit sun owner", root / "linking-owned/owned.npz"),
            ("explicit both owners", root / "linking-owned/both.npz"),
            ("clear reference", root / "linking/clear.npz"),
        ],
    ]
    panel_grid(
        linking,
        root / "linking-panels.png",
        "Actual red/green suns | old and explicit blocker ownership",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    plots(parser.parse_args().input)
