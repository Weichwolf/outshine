"""Replay an archived complete group through production native field jobs and an offline hit."""

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

from compare_building_originals import stage
from field_background_inputs import DATA
from parent_surface_inputs import open_parent


def run(binary, source, request, output, expected=0):
    command = [str(binary), str(source), str(request), str(output)]
    with output.with_suffix(".log").open("w") as log:
        result = subprocess.run(
            command,
            env=dict(os.environ, MTL_DEBUG_LAYER="1"),
            stdout=log,
            stderr=subprocess.STDOUT,
            check=False,
            timeout=45,
        )
    if result.returncode != expected:
        raise ValueError("native field failed: " + str(output.with_suffix(".log")))
    if expected:
        if (output / "atlas.bin").exists() or (
            output / "native-field-result.json"
        ).exists():
            raise ValueError("an unavailable view produced a field")
        if (
            "building source inputs unavailable"
            not in output.with_suffix(".log").read_text()
        ):
            raise ValueError("the missing view failed for an unrelated reason")
        return None
    return json.loads((output / "native-field-result.json").read_text())


def compare(args):
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if output.exists() or output == temporary or not output.is_relative_to(temporary):
        raise ValueError("fresh explicit system-temp output required")
    output.mkdir()
    binary = args.binary.resolve()
    request = json.loads((DATA / "source-plane-request.json").read_text())
    with open_parent(DATA / "original-inputs.tar.gz") as parent:
        all_cells = sorted(parent.forms)
    cells, summaries = stage(output / "source", request | {"cells": all_cells})
    manifest = json.loads((DATA / "native-cells/manifest.json").read_text())
    complete = [row["cell"] for row in manifest["cells"] if not row["unsupported"]]
    selected = [i for i, cell in enumerate(cells) if cell in complete]
    triangles = sum(
        row["triangles"] for row in manifest["cells"] if row["cell"] in complete
    )
    request["cells"] = complete
    request["limits"] = {
        "producer_bytes": 64 * 1024 * 1024,
        "resident_bytes": 1024 * 1024,
        "field_bytes": 1024 * 1024,
        "samples": request["pixels"] ** 2,
        "sources": len(selected),
        "faces": sum(summaries[i]["faces"] for i in selected),
        "triangle_attempts": 4 * triangles,
    }
    request_path = output / "request.json"
    request_path.write_text(json.dumps(request, indent=2) + "\n")
    source = output / "source/input.json"
    miss = run(binary, source, request_path, output / "miss")
    before, after = (miss[key] for key in ("source_costs_before", "source_costs_after"))
    if not miss["native_sources_present"] or not miss["covered"]:
        raise ValueError("the native miss produced no actual covered source field")
    if after["form_hits"] - before["form_hits"] != len(complete):
        raise ValueError(
            "native generation did not read exactly its selected form cells"
        )
    if after["form_writes"] != before["form_writes"]:
        raise ValueError("a prepared source hit rebuilt source forms")
    hit_request = request | {
        "source": miss["source"],
        "cache": str(output / "miss/fields.sqlite"),
    }
    hit_path = output / "hit-request.json"
    hit_path.write_text(json.dumps(hit_request, indent=2) + "\n")
    (output / "source").rename(output / "source-held")
    (output / "miss/native").rename(output / "native-held")
    try:
        if source.exists():
            raise ValueError("the offline source descriptor still exists")
        hit = run(binary, source, hit_path, output / "hit")
        if len(request["directions"]) < 2:
            raise ValueError(
                "the independent unavailable-view control needs two catalogue views"
            )
        absent = hit_request | {"view": 1}
        absent_path = output / "missing-view-request.json"
        absent_path.write_text(json.dumps(absent, indent=2) + "\n")
        run(binary, source, absent_path, output / "missing-view", expected=1)
    finally:
        (output / "source-held").rename(output / "source")
        (output / "native-held").rename(output / "miss/native")
    if hit["native_sources_present"] or any(hit["source_costs_after"].values()):
        raise ValueError("the fresh offline hit touched native sources")
    first, replay = (
        (output / name / "atlas.bin").read_bytes() for name in ("miss", "hit")
    )
    if first != replay or miss["world_from_field"] != hit["world_from_field"]:
        raise ValueError(
            "the native field or intrinsic frame changed across cache restart"
        )
    result = {
        "selected_cells": complete,
        "selected_sources": len(selected),
        "reference_triangles": triangles,
        "atlas_bytes_exact": True,
        "atlas_sha256": hashlib.sha256(first).hexdigest(),
        "fresh_offline_hit_without_sources": True,
        "unavailable_other_view_refused": True,
        "miss": miss,
        "hit": hit,
        "scope": "Actual BuildingFieldRequest/metadata/field/CPU lease path. Archive import and legacy migration precede measured jobs. Producer reservations and sampled host heap do not prove total producer/GPU/driver peaks. No original-pixel, pose, World, frame-p99 or 8GB admission.",
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "selected_sources",
                    "reference_triangles",
                    "atlas_bytes_exact",
                )
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    compare(parser.parse_args())
