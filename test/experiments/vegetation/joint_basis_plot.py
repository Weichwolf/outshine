"""Show actual native source fields, angular loss and rank loss separately."""

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def run(source, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private image must be under/tmp")
    report = json.loads((source / "report.json").read_text())
    if report["angular_coordinates"] != "view" or report["knot_count"] != 64:
        raise ValueError("this comparison needs the aligned64 actual source study")
    with np.load(source / "fields.npz") as archive:
        fields = {k: archive[k] for k in archive.files}
    with np.load(source / "reduction.npz") as archive:
        reduced = {k: archive[k] for k in archive.files}
    truth = reduced["truth"].reshape(3, 256, 8)[:, :, 3]
    angular = reduced["angular_capped"].reshape(3, 256, 8)[:, :, 3]
    positive = reduced["positive8"].reshape(3, 256, 8)[:, :, 3]
    signed = reduced["signed12"].reshape(3, 256, 8)[:, :, 3]
    titles = [
        "Coverage W",
        "No self-shadow",
        "Joint source",
        "64-light interpolation",
        "Positive rank8",
        "Signed rank12, clipped",
        "Signed minus source",
    ]
    fig, axes = plt.subplots(3, 7, figsize=(16, 9), dpi=160, layout="constrained")
    for view in range(3):
        values = [
            fields[f"coverage{view}"],
            fields[f"unshadowed{view}"][67],
            truth[view],
            angular[view],
            positive[view],
            signed[view],
            signed[view] - truth[view],
        ]
        for column, value in enumerate(values):
            ax = axes[view, column]
            is_error = column == 6
            image = ax.imshow(
                value.reshape(16, 16),
                origin="lower",
                interpolation="nearest",
                cmap="coolwarm" if is_error else "viridis",
                vmin=-0.12 if is_error else 0,
                vmax=0.12 if is_error else 1,
            )
            ax.set(xticks=[], yticks=[])
            if view == 0:
                ax.set_title(titles[column], fontsize=9)
            if column == 0:
                ax.set_ylabel(f"Camera {view}", fontsize=10)
            if view == 2 and column in (0, 6):
                fig.colorbar(image, ax=ax, orientation="horizontal", fraction=0.07)
    fig.suptitle(
        "Same 32-needle geometry, 5° light-camera angle; actual native 16×16 fields\n"
        "Quadrature order4 · opaque direct diffuse · no density/albedo fit · no PBR or whole-tree admission",
        fontsize=13,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output)
    plt.close(fig)
    output.with_suffix(".json").write_text(
        json.dumps(
            {
                "image_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                "source": str(source),
                "source_sha256": report["source_sha256"],
                "field_pixels": [16, 16],
                "image_pixels": [2560, 1440],
                "heldout_index": 3,
                "light_camera_degrees": 5,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.output)
