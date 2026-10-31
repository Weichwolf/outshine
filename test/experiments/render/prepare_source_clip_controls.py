"""Export exact synthetic view words, whole source and explicit empty control."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from compare_source_hierarchy import changed_frame
from experiment_output import create_output
from native_source_triangle_materials import DATA, bound_selection
from native_world_inputs import load
from source_clip_demand import query
from source_hierarchy import prepare
from source_hierarchy import query as old_query
from source_triangle_materials import write_source


def export_controls(source, hierarchy, archive, raster, output):
    create_output(output)
    original = output / "whole"
    empty = output / "empty"
    write_source(source, np.arange(len(source["indices"]), dtype=np.uint32), original)
    write_source(source, np.empty(0, np.uint32), empty, referenced_vertices=True)
    placements = archive["placements/words"]
    placements.tofile(output / "placements.bin")
    base = archive["near-direction/view_words"]
    rows = []
    for angle in (*range(0, 360, 30), 63.5):
        words = changed_frame(base, angle, np.zeros(3))
        before = old_query(hierarchy, words, placements, raster, raster_guard_px=0.0)
        ids, work = query(hierarchy, words, placements, raster, raster_guard_px=0.0)
        name = "angle-" + str(angle)
        file = output / (name + "-view-words.bin")
        words.tofile(file)
        rows.append(
            {
                "name": name,
                "synthetic_angle_deg": angle,
                "view_words_file": file.name,
                "view_words_sha256": hashlib.sha256(file.read_bytes()).hexdigest(),
                "view_words_dtype": "<u4",
                "view_words_shape": [56],
                "old_selected": len(before[0]),
                "old_reason": before[1]["reason"],
                "selected": len(ids),
                "complete_model": work["complete_model"],
                "reason": work["reason"],
                "proof": work["clip"],
                "native_control": "whole-vs-no-source-draw"
                if work["reason"] == "source_outside_xy_clip"
                else "whole-required-control",
            }
        )
    result = {
        "version": 1,
        "source_triangles": len(source["indices"]),
        "source_vertices": len(source["positions"]),
        "raster": list(raster),
        "shader_words": "56xUInt32 View,1x36xUInt32 placements, column-major matrices",
        "placements_sha256": hashlib.sha256(
            (output / "placements.bin").read_bytes()
        ).hexdigest(),
        "source_original": "whole/geometry/0/input.json",
        "empty_original_slots": "empty/geometry/0/input.json",
        "scope": "Synthetic exact word requests, not new native uploads or native image evidence. Source unchanged. Current MaterialFieldInput::Prepare rejects empty Geometry; native owner must compare whole with clear/no source draw in the SAME pass. Preserve additional terrain/opening composition. No new GPU ABI or runtime format.",
        "cases": rows,
    }
    result["geometry_files"] = {
        str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(output.glob("*/geometry/0/**/*"))
        if p.is_file()
    }
    (output / "controls.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    m, _, _, source = load(DATA, attributes=True)
    _, binding = bound_selection(DATA, source)
    h = prepare(source)
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as archive:
        result = export_controls(source, h, archive, m["raster"], args.output)
    result["source_native_packet_sha256"] = binding["samples_sha256"]
    (args.output / "controls.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "controls": str(args.output / "controls.json"),
                "cases": len(result["cases"]),
                "outside_cases": sum(
                    c["reason"] == "source_outside_xy_clip" for c in result["cases"]
                ),
            }
        )
    )


if __name__ == "__main__":
    main()
