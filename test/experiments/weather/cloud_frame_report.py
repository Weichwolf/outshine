"""Present actual C++ byte-consumer evidence; outputs stay in system temp."""

import argparse
import json
import statistics
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from cloud_wind_native_report import fields, motion
from matplotlib.backends.backend_pdf import PdfPages


def producer_page(pdf, record, costs):
    density = max(
        row["max_normalized_density_error"] for row in record["fractional_anchor_cases"]
    )
    step = max(
        row["relative_step_l2_error"]
        for row in record["fractional_anchor_cases"]
        if row["relative_step_l2_error"] is not None
    )
    samples = [json.loads(line) for line in costs.read_text().splitlines()]
    fig, ax = plt.subplots(figsize=(11.7, 7.4))
    ax.axis("off")
    ax.text(
        0.02,
        0.96,
        "Production C++ CloudFrame -> unchanged Vulkan density consumer",
        fontsize=17,
        va="top",
    )
    lines = [
        f"{len(record['producer_frames'])} actual C++ payloads; no Python split packing in candidate captures.",
        "112 bytes, alignment16; seven reflected16-byte fields at0/16/32/48/64/80/96.",
        "BroadBase and DetailBase retain unsigned integer bits. Physical shell coordinates stay unchanged.",
        "24 additional fractional-anchor captures include positive/negative month phases and30/60Hz steps.",
        f"Fractional anchors: max normalized density error {density:.3g}; max step L2 error {step:.3%}.",
        "Reference remains independent FP64 world field; limits1e-4 density and5% temporal L2 unchanged.",
        "The C++ offset uses the exact float anchor carried by the packet; both frequencies split independently.",
        "Invalid/nonfinite inputs, overflow and fraction-to-one carry are checked before integer conversion.",
        "",
        f"CPU: {len(samples)} normal samples,100000 frames each; median {statistics.median(row['ns_per_frame'] for row in samples):.2f} ns/frame.",
        "Shared interactive Linux host with background work; this is a component microcost, no device budget.",
        "Fixed work: three axes, six lattice splits/floors,112 output bytes; no world-size traversal or retained heap.",
        "All process/file/GPU harness costs are separate; DHAT and object symbol evidence accompany the receipt.",
        "GPU algorithm/packet unchanged from #456; its costs stay attached to that source and device.",
        "",
        "Open: shared Check.h analysis findings, productive shader/sky/velocity/history connection,",
        "moving world images, upload/residency lifetime and Metal/A18/whole-frame acceptance.",
    ]
    ax.text(0.02, 0.87, "\n\n".join(lines), fontsize=10, va="top")
    pdf.savefig(fig)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--costs", type=Path, required=True)
    args = parser.parse_args()
    record = json.loads((args.folder / "receipt.json").read_text())
    with PdfPages(args.folder / "native-cpp-frame.pdf") as pdf:
        producer_page(pdf, record, args.costs)
        fields(pdf, record)
        motion(pdf, record, args.folder)


if __name__ == "__main__":
    main()
