"""Build and verify a grouped cluster/corner draw plan from a resident snapshot."""

import argparse
import json
from pathlib import Path
import time

import numpy as np

from resident_vertex_references import load, measure


def plan(streams):
    batches = streams["batches"]
    roots = streams["roots"]
    templates = streams["templates"]
    visible = streams["visible"].reshape(-1)
    kept = streams["kept"].reshape(-1)
    source = streams["source_indices"].reshape(-1)
    original_args = streams["arguments"].reshape(-1, 5)
    group_rows, groups, membership = [], [], []
    batch_group = np.empty(len(batches), dtype=np.uint32)
    active = {}
    for row, batch in enumerate(batches):
        # Singly instanced, clustered, depth-writing main geometry only.
        # Motion remains part of the key; deforming groups retain their previous stream.
        eligible = batch[4] != 2 and batch[5] == 1 and batch[10] > 0 and batch[11] < 2
        key = tuple(map(int, batch[[0, 1, 2, 3, 4, 11]]))
        group = active.get(key) if eligible else None
        if group is None:
            group = len(groups)
            groups.append({"source_row": row, "pull": bool(eligible)})
            membership.append([])
            if eligible:
                active[key] = group
        if not eligible:
            active.clear()
        membership[group].append(row)
        batch_group[row] = group
    order = np.argsort(batch_group[roots[:, 7]], kind="stable")
    sorted_roots = roots[order].copy()
    sorted_roots[:, 7] = batch_group[roots[order, 7]]
    logical_count = sum(map(int, sorted_roots[:, 5]))
    corner_max = max(map(int, templates[:, 2]), default=0)
    shift = max(0, corner_max - 1).bit_length()
    if shift > 32 or logical_count > (1 << (32 - shift)):
        raise ValueError("cluster/corner addresses exceed uint32; another representation required")
    references = np.empty((logical_count, 2), dtype=np.uint32)
    chunks = [[] for _ in groups]
    raw_chunks = [[] for _ in groups]
    group_capacities = [0 for _ in groups]
    sorted_kept = []
    logical = 0
    for new_row, old_row in enumerate(order):
        old = roots[old_row]
        first, count, old_logical, old_batch, model, base = map(int, old[4:10])
        group = int(batch_group[old_batch])
        sorted_roots[new_row, 6] = logical
        sorted_kept.append(kept[old_logical:old_logical + count])
        group_capacities[group] += int(old[10])
        for local, item in enumerate(templates[first:first + count]):
            index_first, index_count = base + int(item[1]), int(item[2])
            references[logical] = (index_first, model)
            value = int(kept[old_logical + local]) if visible[old_row] and not int(old[11]) & 4 else 0
            if value:
                if value != index_count:
                    raise ValueError("selected count disagrees with template")
                expected = source[index_first:index_first + index_count]
                raw_chunks[group].append(expected)
                if groups[group]["pull"]:
                    encoded = (np.uint64(logical) << np.uint64(shift)) + np.arange(index_count, dtype=np.uint64)
                    encoded = encoded.astype(np.uint32)
                    decoded = references[encoded >> shift]
                    actual = source[decoded[:, 0].astype(np.uint64) + (encoded & ((1 << shift) - 1))]
                    if not np.array_equal(actual, expected) or not np.all(decoded[:, 1] == model):
                        raise ValueError("virtual indices changed source vertices or placements")
                    chunks[group].append(encoded)
                else:
                    chunks[group].append(expected)
            logical += 1
    indices = []
    arguments = []
    first_index = 0
    for group, rows in enumerate(membership):
        values = np.concatenate(chunks[group]) if chunks[group] else np.empty(0, dtype=np.uint32)
        expected_count = sum(int(original_args[row, 0]) for row in rows)
        if len(values) != expected_count:
            raise ValueError("grouped counts disagree with original native draw arguments")
        batch = batches[groups[group]["source_row"]]
        first_instance = 0 if groups[group]["pull"] else int(batch[8])
        arguments.append((len(values), int(batch[5]), first_index, 0, first_instance))
        group_rows.append((groups[group]["source_row"], int(groups[group]["pull"])))
        indices.append(values)
        first_index += group_capacities[group]
    return {
        "roots": sorted_roots,
        "kept": np.concatenate(sorted_kept),
        "visible": visible[order],
        "raw_indices": np.concatenate([np.concatenate(parts) for parts in raw_chunks if parts])
                       if any(raw_chunks) else np.empty(0, dtype=np.uint32),
        "references": references,
        "indices": np.concatenate(indices) if indices else np.empty(0, dtype=np.uint32),
        "arguments": np.array(arguments, dtype=np.uint32).reshape(-1, 5),
        "groups": np.array(group_rows, dtype=np.uint32).reshape(-1, 2),
        "batch_group": batch_group,
        "root_order": order.astype(np.uint32),
    }, shift


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    metadata, streams = load(args.directory)
    measure(metadata, streams)
    began = time.perf_counter()
    result, shift = plan(streams)
    elapsed = time.perf_counter() - began
    args.output.mkdir(exist_ok=False)
    for name, values in result.items():
        values.astype("<u4").tofile(args.output / (name + ".bin"))
    report = {"version": 1, "corner_bits": shift, "source": str(args.directory.resolve()),
              "groups": len(result["groups"]), "pull_groups": int(result["groups"][:, 1].sum()),
              "original_batches": len(streams["batches"]), "selected_indices": len(result["indices"]),
              "verified_plan_seconds": elapsed,
              "streams": {name: {"shape": values.shape, "bytes": values.nbytes} for name, values in result.items()}}
    (args.output / "plan.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
