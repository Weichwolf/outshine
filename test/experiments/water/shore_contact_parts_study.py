"""Ready part loading and request deltas for the unchanged canonical contact mesh.

Diagnostic NPZ payloads, no Asset-DB/runtime format or visibility policy proposal.
"""

import argparse
import gc
import io
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from outshine_ref.contracts import LocalFrame
from shore_contact_parts import prepare, select


def read(path):
    payload = path.read_bytes()
    with np.load(io.BytesIO(payload), allow_pickle=False) as archive:
        arrays = dict(archive)
    return arrays, len(payload)


def compact(mono, row):
    first, count = row["range"]
    selected = slice(first, first + count)
    nodes, inverse = np.unique(mono["faces"][selected], return_inverse=True)
    lookup = {int(node): i for i, node in enumerate(nodes)}
    pairs = [
        (lookup[int(node)], int(body))
        for node, body in mono["body_pairs"]
        if int(node) in lookup
    ]
    return {
        "vertices": mono["vertices"][nodes],
        "faces": inverse.reshape(-1, 3).astype("<u4"),
        "source_faces": mono["source_faces"][selected],
        "canonical_faces": mono["canonical_faces"][selected],
        "body_pairs": np.array(pairs, dtype="<u4").reshape(-1, 2),
    }


def pack(products, original, heights, anchor, output, cells):
    field, groups, links = prepare(
        products,
        original,
        heights.shape,
        LocalFrame("WGS84-ENU-h0:" + ",".join(map(str, anchor))),
        cells=cells,
    )
    (output / "parts").mkdir()
    mono_ids, rows, offset, bytes_all_parts = [], [], 0, 0
    for key in groups:
        part = select(field, groups, links, [key])
        name = "parts/p-" + "-".join(map(str, key)) + ".npz"
        np.savez(output / name, **part)
        ids = part["canonical_faces"]
        mono_ids.extend(map(int, ids))
        rows.append(
            {
                "key": list(key),
                "file": name,
                "range": [offset, len(ids)],
                "array_bytes": sum(p.nbytes for p in part.values()),
            }
        )
        offset += len(ids)
        bytes_all_parts += (output / name).stat().st_size
    ids = np.array(mono_ids, dtype="<u4")
    if not np.array_equal(np.sort(ids), np.arange(len(products["terrain_faces"]))):
        raise ValueError("part catalog loses or duplicates canonical facets")
    np.savez(
        output / "mono.npz",
        vertices=products["terrain_vertices"],
        faces=products["terrain_faces"][ids].astype("<u4"),
        source_faces=products["terrain_source_faces"][ids],
        canonical_faces=ids,
        body_pairs=np.c_[products["contacts"]["terrain"], products["contacts"]["body"]],
    )
    np.savez(
        output / "body.npz",
        vertices=products["body_vertices"],
        faces=products["body_faces"],
        parents=products["body_parent_faces"],
    )
    catalog = {
        "frame": field.patches[0].frame.identifier,
        "cells_per_part": cells,
        "parts": rows,
    }
    (output / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
    # Check every complete part against the monolithic source without re-clipping.
    mono, _ = read(output / "mono.npz")
    for row in rows:
        part, _ = read(output / row["file"])
        expected = compact(mono, row)
        for key in part:
            if part[key].tobytes() != expected[key].tobytes():
                raise ValueError(
                    "ready part differs from canonical mono geometry: " + key
                )
    return catalog, {
        "parts": len(rows),
        "all_part_array_bytes": sum(row["array_bytes"] for row in rows),
        "all_part_file_bytes": bytes_all_parts,
        "monolithic_file_bytes": (output / "mono.npz").stat().st_size,
        "shared_body_file_bytes": (output / "body.npz").stat().st_size,
        "catalog_file_bytes": (output / "catalog.json").stat().st_size,
    }


def sequence(mode, root, catalog, requests):
    rows = {tuple(r["key"]): r for r in catalog["parts"]}
    body, body_bytes = read(root / "body.npz")
    cached, full, steps = {}, None, []
    for requested in requests:
        start = time.process_time()
        payload_bytes, files, copied = 0, 0, 0
        if mode == "resident":
            if full is None:
                full, payload_bytes = read(root / "mono.npz")
                files = 1
            held = sum(p.nbytes for p in full.values())
        elif mode == "compact":
            full, payload_bytes = read(root / "mono.npz")
            cached = {key: compact(full, rows[key]) for key in requested}
            copied = sum(
                p["vertices"].nbytes + p["faces"].nbytes + p["body_pairs"].nbytes
                for p in cached.values()
            )
            # Detach source/face-ID views before releasing the full payload.
            for part in cached.values():
                for key in ("source_faces", "canonical_faces"):
                    part[key] = part[key].copy()
                    copied += part[key].nbytes
            full = None
            files = 1
            held = sum(p.nbytes for part in cached.values() for p in part.values())
        elif mode == "ready":
            next_cached = {}
            for key in requested:
                if key in cached:
                    next_cached[key] = cached[key]
                else:
                    part, read_bytes = read(root / rows[key]["file"])
                    next_cached[key] = part
                    payload_bytes += read_bytes
                    files += 1
            cached = next_cached
            held = sum(p.nbytes for part in cached.values() for p in part.values())
        else:
            raise ValueError("known resident, compact or ready policy required")
        steps.append(
            {
                "process_cpu_s": time.process_time() - start,
                "payload_file_bytes": payload_bytes,
                "payload_files": files,
                "retained_geometry_array_bytes": held,
                "known_compaction_copy_bytes": copied,
            }
        )
    return {
        "shared_body_file_bytes_once": body_bytes,
        "shared_body_array_bytes": sum(p.nbytes for p in body.values()),
        "steps": steps,
    }


def keys(values):
    if len(values) % 2 or not values:
        raise ValueError("explicit row/column pairs required")
    result = tuple(zip(values[::2], values[1::2], strict=True))
    if len(set(result)) != len(result):
        raise ValueError("duplicate requested part")
    return result


def run(args):
    if args.output.exists():
        raise ValueError("fresh system-temp output required")
    args.output.mkdir(parents=True)
    products, _ = read(args.prepared / "contact.npz")
    water, _ = read(args.prepared / "bank/water-component.npz")
    started = time.process_time()
    catalog, storage = pack(
        products,
        products["original_faces"],
        products["original_height_words"],
        water["anchor"],
        args.output,
        args.cells,
    )
    packing_cpu = time.process_time() - started
    requests = (keys(args.first), keys(args.next))
    if any(
        key not in {tuple(row["key"]) for row in catalog["parts"]}
        for request in requests
        for key in request
    ):
        raise ValueError("requested part does not exist")
    policies = {}
    for mode in ("resident", "compact", "ready"):
        policies[mode] = {
            "normal_runs": [
                sequence(mode, args.output, catalog, requests) for _ in range(7)
            ]
        }
        if args.allocations:
            _, note = allocations(
                args.output / (mode + ".memray"),
                lambda mode=mode: sequence(mode, args.output, catalog, requests),
            )
            policies[mode]["allocations"] = note
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "prepared_commit": json.loads((args.prepared / "report.json").read_bytes())[
            "commit"
        ],
        "requests": requests,
        "storage": storage,
        "packing_process_cpu_s": packing_cpu,
        "policies": policies,
        "scope": __doc__
        + " Every part's position/index/source/alias words match the canonical plan, including body-boundary nodes. The shared closed body loads once for every policy. The supplied requests are explicit geometry demand, not visibility/LOD/shadow/simulation decisions. Warm OS-cache Python NPZ reads after preparation; payload bytes count read_bytes requests, not physical SSD traffic. Resident policy retains the full payload and uses index subranges without compaction; it performs no payload read on movement. Compact policy releases the full payload after selecting the same demand. Ready policy retains common parts and reads only additions. Retained byte counts include numeric geometry arrays only; temporary decode storage, Python/catalog/OS memory and GPU lifetime are separate. Archive preparation, source acquisition/generation and GPU/material/normal/physics/World admission are outside reader timings. These diagnostic NPZ formats are not the native asset cache contract.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    display(args.output, catalog, requests, products)
    print(json.dumps(report))
    gc.collect()


def display(root, catalog, requests, products):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    rows = {tuple(row["key"]): row for row in catalog["parts"]}
    figure, axes = plt.subplots(1, 2, figsize=(11, 5), layout="constrained")
    vertices, faces = products["terrain_vertices"], products["terrain_faces"]
    for axis, requested, title in zip(
        axes,
        requests,
        ("First request", "Next request: one shared / one added"),
        strict=True,
    ):
        axis.add_collection(
            PolyCollection(
                vertices[faces, :2],
                facecolors="whitesmoke",
                edgecolors="lightgray",
                linewidth=0.1,
            )
        )
        for key, colour in zip(requested, ("lightblue", "lightgreen"), strict=True):
            part, _ = read(root / rows[key]["file"])
            axis.add_collection(
                PolyCollection(
                    part["vertices"][part["faces"], :2],
                    facecolors=colour,
                    edgecolors="gray",
                    linewidth=0.3,
                )
            )
            centre = part["vertices"][:, :2].mean(axis=0)
            axis.text(*centre, str(key), ha="center")
            contact = part["vertices"][part["body_pairs"][:, 0], :2]
            axis.scatter(*contact.T, color="red", s=5)
        low, high = vertices[:, :2].min(axis=0), vertices[:, :2].max(axis=0)
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            aspect="equal",
            title=title,
            xlabel="East / m",
            ylabel="North / m",
        )
    figure.suptitle(
        "Canonical contact geometry / ready parts / shared body-node aliases"
    )
    figure.savefig(root / "parts.png", dpi=140)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cells", type=int, default=8)
    parser.add_argument("--first", type=int, nargs="+", required=True)
    parser.add_argument("--next", type=int, nargs="+", required=True)
    parser.add_argument("--allocations", action="store_true")
    run(parser.parse_args())
