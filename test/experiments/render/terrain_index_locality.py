"""Compare unchanged terrain-skirt triangle orders; models are not hardware proof."""

import argparse
import array
import ctypes as C
import ctypes.util
import hashlib
import json
import re
from collections import Counter
from pathlib import Path
from time import perf_counter_ns

ROOT = Path(__file__).resolve().parents[3]


def edge_quads(side):
    if side < 2:
        raise ValueError("a grid needs at least two nodes per side")
    edges = [[] for _ in range(4)]
    at = lambda x, y: y * side + x
    for k in range(side - 1):
        ends = ((at(k, 0), at(k + 1, 0)), (at(k + 1, side - 1), at(k, side - 1)), (at(0, k + 1), at(0, k)), (at(side - 1, k), at(side - 1, k + 1)))
        for edge, (a, b) in enumerate(ends):
            drop = side * side + edge * side
            da, db = (drop + k, drop + k + 1) if edge in (0, 3) else (drop + k + 1, drop + k)
            edges[edge].append((a, b, da, b, db, da))
    return edges


def index_order(side, policy):
    edges = edge_quads(side)
    if policy == "interleaved":
        quads = (edges[e][k] for k in range(side - 1) for e in range(4))
    elif policy == "edge_runs":
        quads = (quad for edge in edges for quad in edge)
    elif policy == "perimeter":
        quads = (quad for edge in (edges[0], edges[3], reversed(edges[1]), reversed(edges[2])) for quad in edge)
    else:
        raise ValueError(policy)
    return array.array("I", (i for quad in quads for i in quad))


def oriented_triangles(indices):
    if len(indices) % 3:
        raise ValueError("incomplete triangle")
    return Counter(tuple(indices[i:i + 3]) for i in range(0, len(indices), 3))


def cache_misses(indices, capacity, lru):
    cache = []
    misses = 0
    for vertex in indices:
        if vertex in cache:
            if lru:
                cache.remove(vertex)
                cache.append(vertex)
        else:
            misses += 1
            cache.append(vertex)
            if len(cache) > capacity:
                cache.pop(0)
    return misses


def batch_vertices(indices, triangle_limit):
    width = triangle_limit * 3
    return sum(len(set(indices[i:i + width])) for i in range(0, len(indices), width))


class CacheStatistics(C.Structure):
    _fields_ = [("vertices_transformed", C.c_uint), ("warps_executed", C.c_uint), ("acmr", C.c_float), ("atvr", C.c_float)]


def meshoptimizer():
    name = ctypes.util.find_library("meshoptimizer")
    if not name:
        raise RuntimeError("installed meshoptimizer C API required; apt package libmeshoptimizer-dev")
    library = C.CDLL(name)
    optimize = library.meshopt_optimizeVertexCache
    optimize.argtypes = [C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.c_size_t, C.c_size_t]
    optimize.restype = None
    analyze = library.meshopt_analyzeVertexCache
    analyze.argtypes = [C.POINTER(C.c_uint), C.c_size_t, C.c_size_t, C.c_uint, C.c_uint, C.c_uint]
    analyze.restype = CacheStatistics
    if C.sizeof(C.c_uint) != 4 or C.sizeof(CacheStatistics) != 16:
        raise RuntimeError("meshoptimizer C ABI mismatch")
    return name, optimize, analyze


def compare(side):
    library_name, optimize, analyze = meshoptimizer()
    variants = {policy: index_order(side, policy) for policy in ("interleaved", "edge_runs", "perimeter")}
    original = variants["interleaved"]
    source = (C.c_uint * len(original))(*original)
    destination = (C.c_uint * len(original))()
    vertices = side * side + 4 * side
    start = perf_counter_ns()
    optimize(destination, source, len(original), vertices)
    optimization_ns = perf_counter_ns() - start
    variants["meshoptimizer"] = array.array("I", destination)
    results = {}
    expected = oriented_triangles(original)
    for name, indices in variants.items():
        if indices.itemsize != 4 or oriented_triangles(indices) != expected:
            raise ValueError(f"oriented original triangles changed: {name}")
        data = (C.c_uint * len(indices))(*indices)
        results[name] = {
            "indices": len(indices),
            "index_bytes": len(indices) * indices.itemsize,
            "vertex_buffer_bytes": vertices * 3 * 4,
            "unique_referenced_vertices": len(set(indices)),
            "sha256": hashlib.sha256(indices.tobytes()).hexdigest(),
            "fifo_misses": {size: cache_misses(indices, size, False) for size in (8, 16, 24, 32, 64)},
            "lru_misses": {size: cache_misses(indices, size, True) for size in (8, 16, 24, 32, 64)},
            "batch_vertices": {size: batch_vertices(indices, size) for size in (8, 16, 32, 64)},
            "meshopt_model": {f"cache{cache}_warp{warp}_group{group}": analyze(data, len(indices), vertices, cache, warp, group).vertices_transformed for cache, warp, group in ((16, 0, 0), (32, 32, 32), (32, 32, 64), (32, 64, 64))},
        }
    return {"side": side, "library": library_name, "meshoptimizer_single_cold_call_ns": optimization_ns, "variants": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-indices", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")) or output == Path("/tmp"):
        parser.error("a dedicated /tmp output directory is required")
    output.mkdir(parents=True, exist_ok=False)
    constants = ROOT / "src/render/terrain/GroundConstants.inc"
    match = re.search(r"GROUND_INT\(kGroundSide, (\d+)\)", constants.read_text())
    side = int(match.group(1))
    report = {
        "scope": "Exact native skirt-index recipe, four candidate orders. Identical oriented triangle multiset and buffer sizes. Simplified FIFO/LRU/batch/meshoptimizer models, no GPU or frame prediction and no global optimum. Library optimizer used only as experiment comparator, no production dependency.",
        "constants_sha256": hashlib.sha256(constants.read_bytes()).hexdigest(),
        "native_source_sha256": hashlib.sha256((ROOT / "src/render/stages/GroundGrid.cpp").read_bytes()).hexdigest(),
        "actual": compare(side),
        "counterexamples": [compare(s) for s in (2, 3, 5, 9, 17)],
    }
    if args.native_indices:
        native = array.array("I")
        native.frombytes(args.native_indices.read_bytes())
        prefix = array.array("I")
        at = lambda x, y: y * side + x
        for y in range(side - 1):
            for x in range(side - 1):
                prefix.extend((at(x, y), at(x, y + 1), at(x + 1, y + 1), at(x, y), at(x + 1, y + 1), at(x + 1, y)))
        expected = prefix + index_order(side, "interleaved")
        if native != expected:
            raise ValueError("native original indices differ from the experiment recipe")
        report["native_original"] = {"sha256": hashlib.sha256(native.tobytes()).hexdigest(), "indices": len(native), "bytes": len(native) * native.itemsize, "exact_model_match": True}
    (output / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report["actual"], indent=2))


if __name__ == "__main__":
    main()
