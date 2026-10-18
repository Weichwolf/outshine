"""Check every actual Blender wood instance against cached double-precision poses."""

import argparse
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import numpy as np
from blender_preview import material
from fern_material import NORMAL_ATTRIBUTE
from window_portrait_source import source
from window_wood_instances import bind, groups


def check(product, catalog):
    start = time.perf_counter()
    graph, axes, _, _, evidence = source(product, product / "execution", catalog)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with patch("window_portrait_source.wood", side_effect=AssertionError("expansion")):
        report = bind(
            graph, axes, material("reference wood", (0.14, 0.105, 0.037), 0.8)
        )
    records = groups(graph, axes)
    matrices = {order: np.empty((len(ids), 4, 4)) for order, _, ids, *_ in records}
    present = {order: np.zeros(len(ids), dtype=bool) for order, _, ids, *_ in records}
    normals = {}
    bpy.context.view_layer.update()
    for item in bpy.context.evaluated_depsgraph_get().object_instances:
        if not item.is_instance:
            continue
        order = int(item.parent.original.name.rsplit(" ", 1)[1])
        slot = item.persistent_id[1]
        if not 0 <= slot < len(present[order]) or present[order][slot]:
            raise AssertionError("each real cached instance exactly once")
        matrices[order][slot] = np.array(item.matrix_world)
        present[order][slot] = True
        if order not in normals:
            attr = item.object.data.attributes[NORMAL_ATTRIBUTE]
            values = np.empty(len(attr.data) * 3, dtype=np.float32)
            attr.data.foreach_get("vector", values)
            normals[order] = values.reshape(-1, 3)
    max_position, max_normal, vertices, corners = 0.0, 0.0, 0, 0
    for order, mesh, ids, origins, frames, scales in records:
        if not present[order].all():
            raise AssertionError("no cached axis may disappear")
        for first in range(0, len(ids), 1024):
            batch = slice(first, first + 1024)
            matrix = matrices[order][batch]
            actual = (
                np.einsum(
                    "vj,nij->nvi",
                    mesh["vertices"].astype(np.float32),
                    matrix[:, :3, :3],
                )
                + matrix[:, None, :3, 3]
            )
            target = (
                np.einsum("vj,nij->nvi", mesh["vertices"], frames[batch])
                * scales[batch, None]
                + origins[batch, None]
            )
            max_position = max(max_position, float(np.max(np.abs(actual - target))))
            actual = np.einsum(
                "vj,nji->nvi", normals[order], np.linalg.inv(matrix[:, :3, :3])
            )
            actual /= np.linalg.norm(actual, axis=2)[..., None]
            target = np.einsum("vj,nij->nvi", mesh["normals"], frames[batch])
            max_normal = max(max_normal, float(np.max(np.abs(actual - target))))
        vertices += len(ids) * len(mesh["vertices"])
        corners += len(ids) * len(mesh["normals"])
    if max_position > 1e-5 or max_normal > 1e-6:
        raise AssertionError("declared local transform tolerances exceeded")
    return report | {
        "identity": evidence["identity"],
        "actual_instances": sum(int(v.sum()) for v in present.values()),
        "vertices_compared": vertices,
        "corner_normals_compared": corners,
        "position_component_error_max_m": max_position,
        "normal_component_error_max": max_normal,
        "seconds": time.perf_counter() - start,
        "peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "scope": "Actual Blender dependency-graph transforms and stored prototype corner normals; does not prove renderer shader normal transport or image equivalence.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("plant-product", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    result = check(args.plant_product, args.catalog)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
