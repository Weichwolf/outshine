"""Compare material-part inputs restored from source ranges with the original adapter."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA, bound_selection
from source_domain_prepared_view import select_sources
from source_original_range_parts import part_inputs, write_part_inputs
from source_original_ranges import read_ranges
from source_original_ranges_study import measured
from source_patch_inputs import read_bound_sources
from source_triangle_materials import triangle_parts, write_source

ROOT = Path(__file__).resolve().parents[3]


def same_as_adapter(parts, expected, binding):
    offset = 0
    for (part, values, labels), (reference, original) in zip(
        parts, expected, strict=True
    ):
        if part != reference or any(
            values[key].tobytes() != original[key].tobytes() for key in original
        ):
            raise ValueError("Restored original material inputs differ")
        if not np.array_equal(labels, binding[offset : offset + len(labels)]):
            raise ValueError("Original source bindings differ")
        offset += len(labels)
    if offset != len(binding):
        raise ValueError("Every source binding must remain in material order")


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    args.output.mkdir(parents=True)
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    bound_selection(DATA, source)
    bounds_raw = args.bounds.read_bytes()
    view = json.loads(args.view.read_text())
    if hashlib.sha256(bounds_raw).hexdigest() != view["bounds_sha256"]:
        raise ValueError("Actual prepared bounds required")
    with np.load(args.bounds, allow_pickle=False) as z:
        prepared = {k: z[k] for k in ("source_ids", "bounds", "normal")}
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as z:
        views, _ = read_controls(z["near-direction/view_words"])
        placement = z["placements/words"].copy()
    reports = []
    for directory, indexed in ((args.direct, False), (args.indexed, True)):
        metadata = json.loads((directory / "report.json").read_text())
        raw_index = (directory / "index.npy").read_bytes()
        if (
            metadata["source_binding_sha256"] != provenance["source_binding_sha256"]
            or hashlib.sha256(raw_index).hexdigest() != metadata["index_sha256"]
        ):
            raise ValueError("Exact source/index binding required")
        index = np.load(directory / "index.npy", allow_pickle=False)
        for angle in (0, 65, 90):
            selected = select_sources(prepared, views[angle], placement)
            recorded = next(
                row for row in view["cases"] if row["angle_degrees"] == angle
            )
            if set(selected) != set(recorded["selected_source_ids"]):
                raise ValueError("Recorded source selection changed")
            rows, _ = read_ranges(directory, index, selected, indexed=indexed)
            expected = triangle_parts(source, rows["ordinal"], referenced_vertices=True)
            parts, cost = measured(lambda rows=rows: part_inputs(rows, source["parts"]))
            same_as_adapter(
                parts, expected, context["triangle_binding"][rows["ordinal"]]
            )
            reports.append(
                {
                    "encoding": "indexed" if indexed else "direct",
                    "angle": angle,
                    "sources": len(selected),
                    "triangles": len(rows),
                    "vertex_count": sum(part["vertices"] for part, _, _ in parts),
                    "native_array_bytes": sum(
                        a.nbytes for _, values, _ in parts for a in values.values()
                    ),
                    "binding_bytes": sum(b.nbytes for _, _, b in parts),
                    **cost,
                }
            )
            if angle == 65 and indexed:
                write_part_inputs(parts, args.output / "restored")
                write_source(
                    source,
                    rows["ordinal"],
                    args.output / "reference",
                    referenced_vertices=True,
                )
    expected_files = {
        p.relative_to(args.output / "reference")
        for p in (args.output / "reference").rglob("*")
        if p.is_file()
    }
    actual_files = {
        p.relative_to(args.output / "restored")
        for p in (args.output / "restored").rglob("*")
        if p.is_file()
    }
    if expected_files != actual_files or any(
        (args.output / "reference" / p).read_bytes()
        != (args.output / "restored" / p).read_bytes()
        for p in expected_files
    ):
        raise ValueError("Native material input files differ")
    result = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "native_input_files_exact": len(expected_files),
        "cases": reports,
        "scope": "Selected originals and native material-part input words only. Source/index, recorded camera and original part metadata are bound to this finite fixture. View-only demand excludes shadows/simulation. Input files and CPU reconstruction do not establish native raster, live World, PBR or device admission. Traced incremental peaks exclude held inputs and native/OS allocations.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("bounds", "view", "direct", "indexed", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())
