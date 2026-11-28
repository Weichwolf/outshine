"""Export up to sixteen explicitly requested existing terrain pages, read-only and offline."""

import argparse
import hashlib
import json
import tarfile
from pathlib import Path

import numpy as np
from terrain_cached_page import package, page_fractions, select_window


def export(cache, key, window, output):
    """Validate the complete input before creating diagnostic files; never cook or fetch."""
    if output.exists():
        raise ValueError("new explicit output directory required")
    data, provenance = package(cache, key)
    selected, context = select_window(data, *window)
    manifest = {
        **provenance,
        **context,
        "window_zoom_x0_x1_y0_y1": list(window),
        "state": "complete_requested_window",
        "rows": [],
    }
    output.mkdir(parents=True)
    files = []
    for row, heights in selected:
        height_path = output / (row["page"] + ".f32")
        fractions_path = output / (row["page"] + ".fractions")
        height_path.write_bytes(heights)
        np.savetxt(fractions_path, page_fractions(row), fmt="%.17g")
        manifest["rows"].append(
            {
                "row": row,
                "height_file": height_path.name,
                "fractions_file": fractions_path.name,
                "height_sha256": hashlib.sha256(heights).hexdigest(),
                "fractions_sha256": hashlib.sha256(
                    fractions_path.read_bytes()
                ).hexdigest(),
            }
        )
        files.extend((height_path, fractions_path))
    metadata = output / "input.json"
    metadata.write_text(json.dumps(manifest, indent=2) + "\n")
    files.append(metadata)
    archive = output / "pages.tar.xz"
    with tarfile.open(archive, "w:xz") as packed:
        for path in files:
            packed.add(path, arcname=path.name, recursive=False)
    return {
        "rows": len(selected),
        "height_bytes": sum(len(v) for _, v in selected),
        "archive": str(archive),
        "archive_bytes": archive.stat().st_size,
        "archive_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
        "state": manifest["state"],
        "source_stored_sha256": provenance["stored_sha256"],
        "source_native_sha256": provenance["native_sha256"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument(
        "--window",
        type=int,
        nargs=5,
        metavar=("ZOOM", "X0", "X1", "Y0", "Y1"),
        required=True,
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(export(args.cache, args.key, args.window, args.output)))


if __name__ == "__main__":
    main()
