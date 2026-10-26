"""Fresh offline prepared-cell replay and direct finished-product reads."""

import argparse
import json
import time
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.vegetation import HabitatStand
from shapely.geometry.base import BaseGeometry


def run(inputs, output):
    records = []
    for folder in sorted(inputs.iterdir()):
        if not folder.is_dir():
            continue
        start = time.perf_counter()
        with (
            np.load(folder / "coarse-fields.npz", allow_pickle=False) as packet,
            np.load(folder / "coarse-support.npz", allow_pickle=False) as mesh,
            np.load(folder / "near-sites.npz", allow_pickle=False) as near,
        ):
            fields = {k: packet[k] for k in packet.files}
            arrays = {k: mesh[k] for k in mesh.files}
            sites = {k: near[k] for k in near.files}
        loaded = time.perf_counter()
        recipe = json.loads((folder / "coarse-plan.json").read_text())
        with ExitStack() as stack:
            for owner, name in (
                (HabitatStand, "_split"),
                (HabitatStand, "_random"),
                (BaseGeometry, "intersection"),
                (shapely, "normalize"),
            ):
                stack.enter_context(
                    patch.object(owner, name, side_effect=AssertionError(name))
                )
            restored = HabitatStand.from_recipe(recipe)
            counts = [
                restored.field(tuple(int(v) for v in key))["counts"]
                for key in fields["keys"]
            ]
            np.testing.assert_array_equal(counts, fields["counts"])
            assert not restored._sites
        restored_time = time.perf_counter()
        detail = HabitatStand.from_recipe(
            json.loads((folder / "detail-plan.json").read_text())
        )
        key = tuple(json.loads((folder / "summary.json").read_text())["near_key"])
        with patch.object(
            HabitatStand, "_split", side_effect=AssertionError("repeated ancestors")
        ):
            generated = detail.sites(key)
        for name, value in sites.items():
            np.testing.assert_array_equal(generated[name], value)
        records.append(
            {
                "source": folder.name,
                "ready_array_read_s": loaded - start,
                "prepared_cell_restore_check_s": restored_time - loaded,
                "one_near_replay_s": time.perf_counter() - restored_time,
                "finished_array_bytes": sum(
                    a.nbytes
                    for group in (fields, arrays, sites)
                    for a in group.values()
                ),
                "near_replay_sites": len(generated["ids"]),
                "all_checks": "passed",
            }
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(records, indent=2) + "\n")
    print(json.dumps(records))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
