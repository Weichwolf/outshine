"""Copy a complete original native cell, retaining every source and form byte."""

import argparse
import hashlib
import json
import struct
import tempfile
from pathlib import Path

from building_plan_inputs import header, prepared, read_plans, summary
from cached_building_streams import Reader, array
from subset_native_source_input import private_generation_view


def subset_cell(data, cell):
    if not 1 <= cell <= 64:
        raise ValueError("native cell must be in 1..64")
    plans = read_plans(data)
    reader = Reader(data)
    reader.skip(8)
    header(reader)
    for width in (8, 9, 8):
        array(reader, width)
    prefix = reader.at
    count = reader.number("Q")
    records = []
    for _ in range(count):
        begin = reader.at
        prepared(reader)
        records.append(data[begin : reader.at])
    if reader.number("Q") != count:
        raise ValueError("source/summary count mismatch")
    summaries = []
    for _ in range(count):
        begin = reader.at
        summary(reader)
        summaries.append(data[begin : reader.at])
    ordinals = [
        i for i, record in enumerate(plans["records"]) if record["cell"] == cell
    ]
    if not ordinals or reader.at != len(data):
        raise ValueError("complete nonempty cell required")
    size = struct.pack("<Q", len(ordinals))
    result = data[:prefix] + size + b"".join(records[i] for i in ordinals)
    result += size + b"".join(summaries[i] for i in ordinals)
    selected = read_plans(result)
    if [r["cell"] for r in selected["records"]] != [cell] * len(ordinals):
        raise AssertionError("cell membership changed")
    return result, [
        {
            "original_source_ordinal": i,
            "original_source_id": plans["records"][i]["source_id"],
        }
        for i in ordinals
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cell", type=int, required=True)
    parser.add_argument("--error-px", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    original = json.loads(args.input.read_text())
    root = args.input.parent
    index, sources = subset_cell((root / original["index"]).read_bytes(), args.cell)
    form = next(f for f in original["forms"] if f["cell"] == args.cell)
    forms = (root / form["file"]).read_bytes()
    if struct.unpack_from("<IQ", forms) != (0x33435342, len(sources)):
        raise ValueError("original schema3 form block must cover the complete cell")
    view, key = private_generation_view(
        (root / original["view_file"]).read_bytes(), original["view_key"], args.error_px
    )
    out.mkdir(parents=True, exist_ok=False)
    for name, data in (
        ("subset.index", index),
        ("subset.forms", forms),
        ("fine-view.structure", view),
    ):
        (out / name).write_bytes(data)
    manifest = {
        "parent": hashlib.sha256(index + forms).hexdigest(),
        "original_parent": original["parent"],
        "original_cell": args.cell,
        "original_sources": sources,
        "original_input": str(args.input.resolve()),
        "original_form_sha256": hashlib.sha256(forms).hexdigest(),
        "index": "subset.index",
        "view_key": key,
        "view_file": "fine-view.structure",
        "forms": [{"cell": args.cell, "file": "subset.forms"}],
        "cluster_triangles": original["cluster_triangles"],
        "generation_error_px": args.error_px,
        "scope": "Complete original cell sources/forms, explicit original ordinal mapping; private generation control, not runtime admission.",
    }
    (out / "input.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest), flush=True)


if __name__ == "__main__":
    main()
