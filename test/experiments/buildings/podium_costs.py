"""Equal constant upper plans: one shared XY template versus independent storey rebuilding."""

import argparse
import json
import os
from pathlib import Path
from unittest.mock import patch

import shapely
from building_setbacks import candidate
from evidence import revision
from podium_plan import choose, constant_upper, proposal
from rooflight_lamina_probe import measured
from stepped_profile_probe import sources
from stepped_source import base_plan
from verify_podium import compare_xy


def geometry_storage(value):
    seen, found = set(), {}

    def visit(item):
        if id(item) in seen:
            return
        seen.add(id(item))
        if isinstance(item, shapely.Geometry):
            found[id(item)] = item
        elif isinstance(item, dict):
            for child in item.values():
                visit(child)
        elif isinstance(item, (tuple, list)):
            for child in item:
                visit(child)

    visit(value)
    return {
        "unique_geometry_handles": len(found),
        "unique_geometry_wkb_bytes": sum(len(g.wkb) for g in found.values()),
    }


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze podium construction before costs")
    records, load = [], os.getloadavg()
    for index, kind in ((1, "wing"), (2, "wing"), (3, "podium")):
        row = sources()[index]
        base = base_plan(row)
        chosen = choose(base, kind)["form_recipe"]
        proposed = proposal(base, kind, chosen["width_m"], chosen["axis"])
        split = chosen["base_storeys"]
        direct = constant_upper(base, proposed, split)
        separate = constant_upper(base, proposed, split, reuse=False)
        compare_xy(direct, separate)
        entries = []
        for method, reuse in (
            ("shared-upper-template", True),
            ("independent-upper-storeys", False),
        ):
            with patch("podium_plan.candidate", wraps=candidate) as counted:
                constant_upper(base, proposed, split, reuse=reuse)
                calls = counted.call_count
            result, timing = measured(
                lambda p=base, q=proposed, b=split, r=reuse: constant_upper(
                    p, q, b, reuse=r
                )
            )
            entries.append(
                timing
                | geometry_storage(result)
                | {"method": method, "measured_candidate_calls_separate_probe": calls}
            )
        records.append(
            {
                "source_id": row["id"],
                "kind": kind,
                "source_storeys": base["storeys"],
                "upper_storeys": base["storeys"] - split,
                "source_rooms": len(base["rooms"]),
                "chosen_request": chosen,
                "entries": entries,
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "costs.json").write_text(
        json.dumps(
            proof
            | {
                "cases": records,
                "load_start": load,
                "load_end": os.getloadavg(),
                "logical_cpus": os.cpu_count(),
                "scope": "Normal-priority shared-host first/21 subsequent calls and separate traced peak. Exact repeated upper contours/rooms/windows/walkable/elevations; one shared XY template versus independent per-storey clipping. Counter probe and WKB size measurement outside timing. Source/base preparation, candidate search, portal-ID binding, full3D products, material remap/export and engine cache excluded. Geometry handles/serialized WKB are descriptors, not total native heap or memory traffic; no native frame/device result.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    costs(args.output)
