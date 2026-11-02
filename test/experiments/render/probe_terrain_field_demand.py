"""Bound the page workload on explicit planar controls before choosing GPU residency."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from probe_terrain_material_substrate import optical_row
from terrain_field_demand import (
    page_storage,
    plane_samples,
    requested_fields,
    requested_pages,
    storage,
)


def plot_costs(records, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 2, figsize=(11, 4))
    for row in records:
        if row["eye_height_m"] != 60 or row["yaw_deg"] != 0 or "scale_m" not in row:
            continue
        choices = {r["core_texels"]: r for r in row["page_core_controls"]}
        choices[64] = row["paged_64"]
        series = [row, *(choices[core] for core in (32, 64, 128, 256))]
        label = f"{row['scale_m']:.1f} m wavelength"
        axes[0].plot(
            [r["independent_page_texel_bytes"] / 2**20 for r in series],
            "o-",
            label=label,
        )
        axes[1].plot([r["products"] for r in series], "o-")
    for axis in axes:
        axis.set_xticks(
            range(5), ["Whole field", "32 px core", "64 px", "128 px", "256 px"]
        )
        axis.grid(alpha=0.3)
    axes[0].set_ylabel("Independent resident texel payload (MiB)")
    axes[1].set_ylabel("Selected packages")
    axes[0].legend(fontsize=8)
    fig.suptitle("720p planar controls, 60 m camera height; actual recipe wavelengths")
    fig.text(
        0.5,
        0.01,
        "Synthetic flat terrain; no Places, native timing or total memory acceptance",
        ha="center",
        fontsize=8,
    )
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(output / "page-granularity.png", dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private demand evidence belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    palette, layout, source = load_capture(args.capture)
    scales = sorted(
        {
            float(optical_row(palette, layout, row)["recipe"][2])
            for row in range(layout["rows"])
            if 0 < optical_row(palette, layout, row)["coverage"] < 1
            and optical_row(palette, layout, row)["recipe"][2] > 0
        }
    )
    records = []
    for altitude in (1.7, 60, 500, 2000):
        for yaw in (0, 45):
            at, dx, dy, pixels = plane_samples(
                args.width, args.height, altitude, 55, -6, yaw
            )
            at += [-1024.137, -1024.271]
            demands, page_demands = [], []
            for scale in scales:
                began = time.perf_counter()
                keys, active, lod = requested_fields(at, dx, dy, scale)
                elapsed = (time.perf_counter() - began) * 1000
                began = time.perf_counter()
                pages, _, _ = requested_pages(at, dx, dy, scale)
                page_ms = (time.perf_counter() - began) * 1000
                records.append(
                    {
                        "eye_height_m": altitude,
                        "yaw_deg": yaw,
                        "scale_m": scale,
                        "visible_plane_pixels": len(pixels),
                        "active_field_pixels": int(active.sum()),
                        "lod_range": [float(lod.min()), float(lod.max())]
                        if len(lod)
                        else [],
                        "python_ms": elapsed,
                        "paged_64": {"python_ms": page_ms, **page_storage(pages)},
                        **storage(keys),
                    }
                )
                demands.append(keys)
                page_demands.append(pages)
                if altitude == 60 and yaw == 0:
                    choices = []
                    for core in (32, 128, 256):
                        chosen, _, _ = requested_pages(
                            at, dx, dy, scale, core_texels=core
                        )
                        choices.append(
                            {"core_texels": core, **page_storage(chosen, core)}
                        )
                    records[-1]["page_core_controls"] = choices
                    churn = []
                    for travel in (0.01, 0.1, 1, 10):
                        moved, _, _ = requested_fields(at + [travel, 0], dx, dy, scale)
                        moved_pages, _, _ = requested_pages(
                            at + [travel, 0], dx, dy, scale
                        )
                        missing = np.setdiff1d(moved, keys, assume_unique=True)
                        page_missing = np.setdiff1d(
                            moved_pages, pages, assume_unique=True
                        )
                        churn.append(
                            {
                                "travel_m": travel,
                                **storage(missing),
                                "paged_64": page_storage(page_missing),
                            }
                        )
                    records[-1]["translation_misses"] = churn
            union = (
                np.unique(np.concatenate(demands))
                if demands
                else np.empty(0, dtype=np.uint64)
            )
            page_union = (
                np.unique(np.concatenate(page_demands))
                if page_demands
                else np.empty(0, dtype=np.uint64)
            )
            records.append(
                {
                    "eye_height_m": altitude,
                    "yaw_deg": yaw,
                    "case": "union_of_full_frame_material_controls",
                    **storage(union),
                    "paged_64": page_storage(page_union),
                }
            )
    plot_costs(records, output)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "extent": [args.width, args.height],
        "camera": {"fov_deg": 55, "pitch_deg": -6, "radius_m": 240000},
        "python_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "palette": source["palette"],
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("terrain_field_demand.py"),
            )
        },
        "records": records,
        "limitations": [
            "Planar controls using actual recipe wavelengths; not measured Places or DEM/occlusion acceptance.",
            "Each wavelength control covers all visible ground; their union is a stress envelope, not a classified scene.",
            "Independent page payload includes stored halos; overlapping images can share texels in a clipmap/atlas.",
            "Python demand timing is not native CPU/GPU frame-time evidence.",
        ],
    }
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
