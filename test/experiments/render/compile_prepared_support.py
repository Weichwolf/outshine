"""Prepare view-independent Shell support metadata from the delivered native parent."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

from parent_surface_inputs import open_parent
from prepared_source_support import build, store


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ("input", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp")
    start = time.perf_counter()
    with open_parent(args.input) as parent:
        read_s = time.perf_counter() - start
        archive_bytes = args.input.stat().st_size
        index_bytes = parent.packages[parent.metadata["index"]]["native_bytes"]
        start = time.perf_counter()
        arrays, work = build(parent)
        prepare_s = time.perf_counter() - start
        header = store(arrays, parent, output)
    report = {
        "initial_parent_and_index_read_s": read_s,
        "support_prepare_s": prepare_s,
        "archive_bytes_hashed": archive_bytes,
        "decoded_index_bytes": index_bytes,
        "header": header,
        "work": work,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": None,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "terminal": "complete",
                "source_records": work["source_records"],
                "available": work["available"],
                "stored_array_bytes": header["stored_array_bytes"],
                "additional_enclosure_bytes": header["additional_enclosure_bytes"],
                "prepare_s": prepare_s,
            }
        )
    )


if __name__ == "__main__":
    main()
