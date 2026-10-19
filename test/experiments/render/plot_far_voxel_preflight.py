"""Plot observed-cell cost assumptions against actual native Shell bytes."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    out.mkdir(parents=True, exist_ok=False)
    fig, axis = plt.subplots(figsize=(10, 6), layout="constrained")
    rows = []
    for path in sorted(a.input.glob("*/report.json")):
        report = json.loads(path.read_text())
        shell = report["native_meshes"]["shell"]
        x, y = [], []
        for scale in sorted({r["distance_extent_multiple"] for r in report["cases"]}):
            cases = [
                r["voxel_preflight"]
                for r in report["cases"]
                if r["distance_extent_multiple"] == scale
            ]
            valid = [r for r in cases if r["eligible"]]
            entry = {
                "source": path.parent.name,
                "extent_multiple": scale,
                "eligible_views": len(valid),
                "total_views": len(cases),
                "shell_raw_bytes": shell["raw_used_bytes"],
                "shell_triangles": shell["triangles"],
            }
            if valid:
                count = max(r["sampled_nonempty_cells"] for r in valid)
                entry.update(
                    max_observed_cells_per_view=count,
                    optimistic_bytes_per_view=14 * count,
                    assumed_payload_over_shell=14 * count / shell["raw_used_bytes"],
                )
                x.append(scale)
                y.append(entry["assumed_payload_over_shell"])
            rows.append(entry)
        axis.plot(x, y, "o-", label=path.parent.name)
    axis.axhline(1, color="black", linestyle="--", label="Native Shell raw bytes")
    axis.set(
        xscale="log",
        yscale="log",
        xlabel="Orbit distance / source extent",
        ylabel="14 B × max observed cells per view / native Shell raw bytes",
        title="Far-voxel preflight: observed cells only, before shader fitting",
    )
    axis.grid(alpha=0.2, which="both")
    axis.legend(fontsize=8, loc="lower left")
    fig.text(
        0.5,
        -0.015,
        "Not complete voxel assets, residency, rendered appearance or device times. Missing near points reject the 64 m reuse domain.",
        ha="center",
        fontsize=8,
    )
    fig.savefig(out / "observed-cell-cost.png", dpi=180, bbox_inches="tight")
    plt.close(fig)
    (out / "summary.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    main()
