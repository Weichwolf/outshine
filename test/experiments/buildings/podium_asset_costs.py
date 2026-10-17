"""Complete prepared-asset costs and equal warm material-binding alternatives."""

import argparse
import json
import os
import time
import tracemalloc
from collections.abc import Mapping
from dataclasses import fields, is_dataclass
from pathlib import Path

import numpy as np
from evidence import revision
from podium_products import bound, prepare
from rooflight_lamina_probe import measured
from stepped_profile_probe import sources


def storage(value):
    seen, arrays, roots = set(), {}, {}

    def visit(item):
        if id(item) in seen:
            return
        seen.add(id(item))
        if isinstance(item, np.ndarray):
            arrays[id(item)] = item
            root = item
            while isinstance(root.base, np.ndarray):
                root = root.base
            roots[id(root)] = root
        elif isinstance(item, Mapping):
            for child in item.values():
                visit(child)
        elif isinstance(item, (tuple, list)):
            for child in item:
                visit(child)
        elif is_dataclass(item):
            for field in fields(item):
                visit(getattr(item, field.name))

    visit(value)
    return {
        "unique_array_handles": len(arrays),
        "unique_numpy_root_buffers": len(roots),
        "numpy_root_bytes": sum(a.nbytes for a in roots.values()),
    }


def copied(product):
    result = bound(product, 17, 33000)
    for key, value in product.items():
        if key != "materials":
            result[key] = value.copy()
    return result


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze prepared asset source before costs")
    load = os.getloadavg()
    records = []
    for index, kind in ((2, "wing"), (3, "podium")):
        row = sources()[index]
        start = time.perf_counter()
        asset = prepare(row, kind)
        first_ms = (time.perf_counter() - start) * 1000
        tracemalloc.start()
        traced = prepare(row, kind)
        _current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        del traced
        entries = []
        for mode, product in (
            ("near", asset.near_draw),
            ("compact", asset.compact_draw),
        ):
            shared, full = bound(product, 17, 33000), copied(product)
            for key in product:
                np.testing.assert_array_equal(shared[key], full[key])
            for name, operation in (
                ("share-prepared-streams", lambda p=product: bound(p, 17, 33000)),
                ("copy-all-prepared-streams", lambda p=product: copied(p)),
            ):
                result, timing = measured(operation)
                entries.append(
                    timing
                    | {
                        "mode": mode,
                        "method": name,
                        "triangles": len(product["faces"]),
                        "new_array_bytes": sum(
                            a.nbytes
                            for key, a in result.items()
                            if a is not product[key]
                        ),
                    }
                )
        records.append(
            {
                "source_id": row["id"],
                "kind": kind,
                "first_complete_prepare_ms": first_ms,
                "separate_prepare_traced_peak_bytes": peak,
                "retained_numpy": storage(asset),
                "entries": entries,
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "asset-costs.json").write_text(
        json.dumps(
            proof
            | {
                "cases": records,
                "load_start": load,
                "load_end": os.getloadavg(),
                "logical_cpus": os.cpu_count(),
                "scope": "First complete prepared asset per case and separate traced peak; source fixture loaded, imports warm. Warm binding alternatives have identical arrays/values and complete prepared near/compact inputs: first/21 subsequent calls and separate peak. New array bytes count explicit output allocations, not all memory traffic. Retained NumPy roots exclude GEOS/Python/native heap; traced peak is not process RSS. No persistent cache IO, renderer/GPU/frame or device admission.",
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
