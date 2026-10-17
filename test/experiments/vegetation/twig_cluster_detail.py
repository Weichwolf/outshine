"""Actual cached host subset, with one common rigid transform for close inspection."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from crown_pair_export import export_pair
from tree_entry import load
from twig_cluster_experiment import read_plan


def blocks(mesh, start, count):
    vbegin, vend = start * 18, (start + count) * 18
    fbegin, fend = start * 32, (start + count) * 32
    faces = mesh["faces"][fbegin:fend] - vbegin
    vertices = mesh["vertices"][vbegin:vend]
    assert faces.min() >= 0 and faces.max() < len(vertices)
    return {
        "vertices": vertices,
        "faces": faces,
        "normals": mesh["normals"][fbegin * 3 : fend * 3],
        "corner_uv": mesh["corner_uv"][fbegin * 3 : fend * 3],
        "surface": mesh["surface"][fbegin:fend],
    }


def joined(first, second):
    return {
        k: np.concatenate(
            (
                first[k],
                second[k] + len(first["vertices"]) if k == "faces" else second[k],
            )
        )
        for k in first
    }


def build(inputs, output, host_index):
    plan = read_plan(inputs)
    if not 0 <= host_index < len(plan.host_ids):
        raise ValueError("existing explicit host index required")
    node = int(plan.host_ids[host_index])
    n = (plan.host_counts + plan.leaf_limit - 1) // plan.leaf_limit
    _, before = load(inputs / "recursive")
    _, after = load(inputs / "spatial")
    original = before["wood.npz"]
    whole = after["wood.npz"]
    backbone = len(original["vertices"]) // 18 - len(plan.host_ids)
    carrier = blocks(original, backbone + host_index, 1)
    start = len(original["vertices"]) // 18 + int(n[:host_index].sum())
    candidate = joined(carrier, blocks(whole, start, int(n[host_index])))
    first, last = map(int, plan.offsets[host_index : host_index + 2])
    upright = np.array(((1, 0, 0), (0, 0, -1), (0, 1, 0)))
    rotation = upright @ plan.frames[host_index].T
    wood = []
    tables = []
    for mesh, products in ((carrier, before), (candidate, after)):
        wood.append(
            mesh
            | {
                "vertices": (mesh["vertices"] - plan.origins[host_index]) @ rotation.T,
                "normals": mesh["normals"] @ rotation.T,
            }
        )
        rows = {k: a[first:last].copy() for k, a in products["tables.npz"].items()}
        rows["leaf_origins"] = (
            rows["leaf_origins"] - plan.origins[host_index]
        ) @ rotation.T
        rows["leaf_frames"] = rotation @ rows["leaf_frames"]
        tables.append(rows)
    output.mkdir(parents=True, exist_ok=False)
    identity = hashlib.sha256(
        (plan.identity + ":host:" + str(node)).encode()
    ).hexdigest()
    export_pair(
        inputs,
        output,
        *wood,
        tables,
        identity,
        scope="Same cached host subset and physical leaves; common rigid transform only, no modified whole plant.",
    )
    for name, mesh, rows in zip(("recursive", "spatial"), wood, tables):
        path = output / name / "entry.json"
        meta = json.loads(path.read_text())
        count = len(mesh["faces"])
        for v in range(8):
            count += int(np.count_nonzero(rows["leaf_variants"] == v)) * len(
                after[f"blade-{v}.npz"]["faces"]
            )
        source_plan = plan.source_identity if name == "recursive" else plan.identity
        request_identity = hashlib.sha256(
            (source_plan + ":host:" + str(node)).encode()
        ).hexdigest()
        meta.update(
            plan_identity=request_identity,
            instanced_triangles=count,
            source_plan=source_plan,
            host_node=node,
        )
        path.write_text(json.dumps(meta, indent=2) + "\n")
    report = {
        "source_plan": plan.identity,
        "host_node": node,
        "source_host_index": host_index,
        "leaf_ids": tables[0]["leaf_ids"].tolist(),
        "presentation_labels": {
            "recursive": "one crowded carrier",
            "spatial": "same leaves on attached short shoots",
        },
        "presentation_camera": {
            "eye": [0, -3, 1.5],
            "target": [0, 0, 0.14],
            "span_m": 2.0,
            "separation_m": 0.4,
            "ground_size_m": 3,
            "clip_m": [0.01, 20],
        },
        "scope": "Exact source subset, common rigid transform, no crop/rescale of organs.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--host-index", type=int, default=100)
    args = parser.parse_args()
    build(args.inputs, args.output, args.host_index)
