"""Compare shared object/point instances with identical measured rigid poses."""

import json
import resource
import statistics
import time
from pathlib import Path

import bpy
import numpy as np
from blender_instances import instance_group
from mathutils import Matrix
from scipy.spatial import cKDTree
from scipy.spatial.transform import Rotation

OUTPUT = Path("build/experiments/vegetation/europe-conifers/instancing.json")


def prototype():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    mesh = bpy.data.meshes.new("asymmetric reference")
    mesh.from_pydata(
        [(0, 0, 0), (0.003, 0.025, 0.004), (-0.002, 0.018, 0.007)], [], [(0, 1, 2)]
    )
    obj = bpy.data.objects.new("shared prototype", mesh)
    bpy.context.collection.objects.link(obj)
    obj.hide_render = True
    return obj


def objects(proto, origins, frames, scales):
    for origin, frame, scale in zip(origins, frames, scales):
        obj = proto.copy()
        obj.name = "reference object"
        obj.hide_render = False
        bpy.context.collection.objects.link(obj)
        transform = np.eye(4)
        transform[:3, :3], transform[:3, 3] = frame * scale, origin
        obj.matrix_world = Matrix(transform.tolist())


def evaluated(method):
    graph = bpy.context.evaluated_depsgraph_get()
    matrices = []
    for item in graph.object_instances:
        if (
            method == "points" and item.is_instance and item.parent.name == "points"
        ) or (method == "objects" and item.object.name.startswith("reference object")):
            matrices.append(np.asarray(item.matrix_world).copy())
    return np.array(matrices)


def cleanup(proto):
    for obj in tuple(bpy.data.objects):
        if obj != proto:
            bpy.data.objects.remove(obj, do_unlink=True)
    for collection in (bpy.data.meshes, bpy.data.node_groups):
        for data in tuple(collection):
            if data.users == 0:
                collection.remove(data)
    bpy.context.view_layer.update()


def measure(proto, origins, frames, scales, method):
    began = time.perf_counter()
    if method == "points":
        instance_group(proto, origins, frames, scales, name="points")
    else:
        objects(proto, origins, frames, scales)
    built = time.perf_counter() - began
    began = time.perf_counter()
    matrices = evaluated(method)
    elapsed = time.perf_counter() - began
    if len(matrices) != len(origins):
        raise AssertionError("different evaluated instance counts")
    distance, index = cKDTree(origins).query(matrices[:, :3, 3])
    if len(np.unique(index)) != len(origins):
        raise AssertionError("evaluated origins lost or duplicated")
    expected = frames[index] * scales[index, None, None]
    error = float(np.max(abs(matrices[:, :3, :3] - expected)))
    if distance.max() > 2e-7 or error > 2e-6:
        raise AssertionError("evaluated transform differs from physical pose")
    result = {
        "method": method,
        "build_ms": 1000 * built,
        "evaluation_and_readback_ms": 1000 * elapsed,
        "objects": len(bpy.data.objects),
        "max_position_error_m": float(distance.max()),
        "max_matrix_error": error,
        "evaluated_instances": len(matrices),
    }
    cleanup(proto)
    return result


def rotation_comparison(frames):
    samples = {"scipy": [], "mathutils": []}
    error = 0
    for repeat in range(5):
        for method in (
            ("scipy", "mathutils") if repeat % 2 == 0 else ("mathutils", "scipy")
        ):
            began = time.perf_counter()
            values = (
                Rotation.from_matrix(frames).as_euler("xyz")
                if method == "scipy"
                else np.array([Matrix(f.tolist()).to_euler("XYZ") for f in frames])
            )
            samples[method].append(1000 * (time.perf_counter() - began))
            replay = Rotation.from_euler("xyz", values).as_matrix()
            error = max(error, float(np.max(abs(replay - frames))))
    if error > 2e-6:
        raise AssertionError("standard rotation conversions do not preserve frames")
    return {
        "samples_ms": samples,
        "median_ms": {k: statistics.median(v) for k, v in samples.items()},
        "max_reconstructed_matrix_error": error,
    }


def main():
    count, rng = 2000, np.random.default_rng(417)
    frames = Rotation.random(count, random_state=rng).as_matrix()
    origins, scales = rng.uniform(-1, 1, (count, 3)), rng.uniform(0.8, 1.2, count)
    proto = prototype()
    samples = []
    for repeat in range(5):
        for method in (
            ("points", "objects") if repeat % 2 == 0 else ("objects", "points")
        ):
            samples.append(measure(proto, origins, frames, scales, method))
    result = {
        "blender": bpy.app.version_string,
        "count": count,
        "samples": samples,
        "rotation": rotation_comparison(frames),
        "median": {
            m: {
                k: statistics.median(s[k] for s in samples if s["method"] == m)
                for k in ("build_ms", "evaluation_and_readback_ms")
            }
            for m in ("points", "objects")
        },
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "CPU preview construction/readback with one shared triangle, not runtime/GPU proof",
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: result[k] for k in ("count", "median", "rotation")}, indent=2))


if __name__ == "__main__":
    main()
