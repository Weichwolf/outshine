"""Emit shared complete prototypes and placements from requested generator nodes."""

import hashlib
import json

import numpy as np
from bark_recipe import bark_recipe
from generator_graph import MAX_POSES
from graph_sources import source_parts, source_triangles


def pose_arrays(poses):
    lengths = np.array([len(p.path) for p in poses], dtype=np.uint64)
    return {
        "origins": np.array([p.origin for p in poses]),
        "frames": np.array([p.frame * p.scale for p in poses]),
        "path_offsets": np.concatenate(
            (np.zeros(1, dtype=np.uint64), np.cumsum(lengths))
        ),
        "path_pairs": np.array(
            [entry for p in poses for entry in p.path], dtype=np.uint32
        ),
    }


def graph_products(
    graph,
    output,
    detail="near",
    nodes=None,
    max_new_poses=MAX_POSES,
    max_prototype_triangles=1_000_000,
):
    selected = (
        tuple(key for key in graph.order if graph.by_id[key].kind != "root")
        if nodes is None
        else tuple(nodes)
    )
    if len(set(selected)) != len(selected) or any(
        key not in graph.by_id or graph.by_id[key].kind == "root" for key in selected
    ):
        raise ValueError("distinct implemented geometry nodes required")
    if any(
        type(value) is not int or value < 0
        for value in (max_new_poses, max_prototype_triangles)
    ):
        raise ValueError("nonnegative integer source and pose budgets required")
    sources, missing = {}, set()
    for key in selected:
        if graph.counts[key] == 0:
            continue
        node = graph.by_id[key]
        identity = hashlib.sha256(
            json.dumps(
                {"kind": node.kind, "shape": node.recipe()["shape"], "detail": detail},
                sort_keys=True,
            ).encode()
        ).hexdigest()
        sources.setdefault(identity, {"shape": node.shape, "nodes": []})[
            "nodes"
        ].append(key)
        pending = [key]
        while pending:
            current = pending.pop()
            if current in missing or graph.counts[current] == 0:
                continue
            missing.add(current)
            pending.extend(graph.by_id[current].parents)
    total = sum(graph.counts[key] - graph._pose_counts.get(key, 0) for key in missing)
    triangles = sum(
        source_triangles(source["shape"], detail) for source in sources.values()
    )
    if (
        total > max_new_poses
        or len(graph._poses) + total > MAX_POSES
        or any(graph.counts[key] > MAX_POSES for key in selected)
        or triangles > min(max_prototype_triangles, 1_000_000)
    ):
        return None
    output.mkdir(parents=True, exist_ok=True)
    (output / "bark-material.json").write_text(
        json.dumps(bark_recipe(), indent=2) + "\n"
    )
    products, names = [], {}
    for identity, source in sorted(sources.items()):
        prefix = f"source-{identity[:32]}-{detail}"
        if prefix in names and names[prefix] != identity:
            raise ValueError("source filename hash collision")
        names[prefix] = identity
        poses = tuple(
            p for key in source["nodes"] for p in graph.instances(key, max_new_poses)
        )
        placement_name = prefix + "-poses.npz"
        np.savez_compressed(output / placement_name, **pose_arrays(poses))
        parts = source_parts(source["shape"], detail)
        assert sum(len(mesh["faces"]) for mesh in parts.values()) == source_triangles(
            source["shape"], detail
        )
        for part, mesh in parts.items():
            mesh_name = prefix + "-" + part + "-mesh.npz"
            np.savez_compressed(output / mesh_name, **mesh)
            product = {
                "mesh": mesh_name,
                "placements": placement_name,
                "surface": part,
                "triangles_per_instance": len(mesh["faces"]),
                "instances": len(poses),
                "generator_ids": source["nodes"],
                "source_identity": identity,
            }
            if part == "wood":
                product["bark_material"] = "bark-material.json"
            products.append(product)
    return {
        "products": products,
        "prototypes_built": len(sources),
        "prototype_triangles": triangles,
        "pose_count": len(graph._poses),
        "plan": graph.recipe(),
        "plan_identity": graph.identity,
    }
