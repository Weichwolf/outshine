"""Native-attribute/source-preserving QEM control; no native LOD admission (#942)."""

import ctypes
import hashlib
import sys
from pathlib import Path

import numpy as np
from attributed_prisms import native_inputs
from packed_normals import unpack


def installed_library():
    suffix = "dylib" if sys.platform == "darwin" else "so"
    return Path(sys.prefix) / "lib" / f"libmeshoptimizer.{suffix}"


def attribute_abi(library=None):
    """Bind the inspected public 1.3 ABI of an installed standard dependency."""
    library = library or installed_library()
    header = library.parent.parent / "include/meshoptimizer.h"
    if "#define MESHOPTIMIZER_VERSION 1030" not in header.read_text():
        raise ValueError("explicit dependency must be installed meshoptimizer1.3")
    index = ctypes.POINTER(ctypes.c_uint)
    scalar = ctypes.POINTER(ctypes.c_float)
    size = ctypes.c_size_t
    expected = [
        index,
        index,
        size,
        scalar,
        size,
        size,
        scalar,
        size,
        scalar,
        size,
        ctypes.POINTER(ctypes.c_ubyte),
        size,
        ctypes.c_float,
        ctypes.c_uint,
        scalar,
    ]
    function = ctypes.CDLL(str(library)).meshopt_simplifyWithAttributes
    if function.argtypes is not None and function.argtypes != expected:
        raise ValueError("unexpected attribute-simplifier ABI")
    function.argtypes, function.restype = expected, size
    return function, library


def coupled_vertices(source):
    """Weld only bit-identical complete attribute corners with the same lineage.

    This deliberately keeps UV, normal and categorical/source seams. The
    simplifier references old vertices and never averages categorical words.
    """
    validate_source(source)
    faces = source["indices"]
    inputs = native_inputs(source)
    lineage, owner = np.unique(source["binding"][:, :5], axis=0, return_inverse=True)
    corner_owner = np.repeat(owner, 3).astype(np.uint32)
    corner_codes = np.repeat(inputs["flat_codes"], 3).astype(np.int32)
    records = [
        source[name][faces].reshape(-1, source[name].shape[1]).view(np.uint32)
        for name in ("positions", "uv", "colour", "metres")
    ]
    key = np.column_stack(
        [
            *records,
            source["normal"][faces].ravel(),
            corner_owner,
            corner_codes.view(np.uint32),
        ]
    )
    _, first, inverse = np.unique(key, axis=0, return_index=True, return_inverse=True)
    originals = faces.ravel()[first]
    attributes = np.column_stack(
        [
            unpack(source["normal"][originals]),
            inputs["uv"][originals],
            source["metres"][originals],
            source["colour"][originals],
        ]
    ).astype(np.float32)
    return {
        "positions": np.ascontiguousarray(source["positions"][originals]),
        "indices": inverse.astype(np.uint32),
        "attributes": np.ascontiguousarray(attributes),
        "original_vertices": originals,
        "lineage": lineage,
        "vertex_lineage": corner_owner[first],
        "vertex_code": corner_codes[first],
        "key_bytes": key.nbytes,
    }


def categorical_locks(prepared):
    positions = prepared["positions"].view(np.uint32).copy()
    positions[(positions & 0x7FFFFFFF) == 0] = 0
    _, position = np.unique(positions, axis=0, return_inverse=True)
    categories = np.unique(
        np.column_stack(
            [position, prepared["vertex_lineage"], prepared["vertex_code"]]
        ),
        axis=0,
    )
    counts = np.bincount(
        categories[:, 0].astype(int), minlength=int(position.max()) + 1
    )
    return np.where(counts[position] > 1, 2, 0).astype(np.uint8)


def validate_source(source):
    vertices, faces = source["positions"], source["indices"]
    if (
        vertices.ndim != 2
        or vertices.shape[1:] != (3,)
        or not 0 < len(vertices) <= 262144
        or faces.ndim != 2
        or faces.shape[1:] != (3,)
        or not 0 < len(faces) <= 131072
        or faces.dtype.kind not in "iu"
        or faces.min() < 0
        or faces.max() >= len(vertices)
        or source["binding"].shape != (len(faces), 6)
        or source["binding"].dtype != np.int32
    ):
        raise ValueError("bounded valid native vertices/faces/lineage required")
    for name, width in (("positions", 3), ("uv", 2), ("metres", 2), ("colour", 4)):
        array = source[name]
        if (
            array.dtype != np.float32
            or array.shape != (len(vertices), width)
            or not np.isfinite(array).all()
        ):
            raise ValueError("finite Float32 native attribute arrays required")
    if source["normal"].dtype != np.uint32 or source["normal"].shape != (
        len(vertices),
    ):
        raise ValueError("native packed Uint32 normal array required")


def simplify(
    source,
    *,
    absolute_error_m=0.001,
    target_fraction=0.25,
    library=None,
    permissive=False,
):
    if (
        not np.isfinite(absolute_error_m)
        or absolute_error_m < 0
        or not 0 < target_fraction <= 1
    ):
        raise ValueError(
            "finite nonnegative metric tolerance and positive target fraction required"
        )
    function, library = attribute_abi(library)
    prepared = coupled_vertices(source)
    positions, indices, attributes = (
        prepared[k] for k in ("positions", "indices", "attributes")
    )
    destination = np.empty_like(indices)
    error = np.zeros(1, np.float32)
    weights = np.ones(attributes.shape[1], np.float32)
    locks = (
        categorical_locks(prepared)
        if permissive
        else np.zeros(len(positions), np.uint8)
    )
    index_pointer, scalar = (
        ctypes.POINTER(ctypes.c_uint),
        ctypes.POINTER(ctypes.c_float),
    )
    count = function(
        destination.ctypes.data_as(index_pointer),
        indices.ctypes.data_as(index_pointer),
        len(indices),
        positions.ctypes.data_as(scalar),
        len(positions),
        positions.strides[0],
        attributes.ctypes.data_as(scalar),
        attributes.strides[0],
        weights.ctypes.data_as(scalar),
        attributes.shape[1],
        locks.ctypes.data_as(ctypes.POINTER(ctypes.c_ubyte)),
        max(3, int(len(indices) * target_fraction) // 3 * 3),
        absolute_error_m,
        4 | (32 if permissive else 0),
        error.ctypes.data_as(scalar),
    )
    if count > len(indices) or count % 3:
        raise ValueError("invalid simplifier index count")
    selected = destination[:count].reshape(-1, 3)
    if selected.size and selected.max() >= len(positions):
        raise ValueError("simplifier returned an unknown vertex")
    for name in ("vertex_lineage", "vertex_code"):
        values = prepared[name][selected]
        if not np.all(values == values[:, :1]):
            raise ValueError(f"simplification crossed a categorical {name} boundary")
    compact, inverse = np.unique(selected, return_inverse=True)
    originals = prepared["original_vertices"][compact]
    result = {
        name: source[name][originals].copy()
        for name in ("positions", "uv", "normal", "metres", "colour")
    }
    result["indices"] = inverse.reshape(-1, 3).astype(np.uint32)
    owners = prepared["vertex_lineage"][selected[:, 0]]
    result["binding"] = np.column_stack(
        [prepared["lineage"][owners], np.full(len(selected), -1, np.int32)]
    ).astype(np.int32)
    result["vertex_normal"] = unpack(result["normal"])
    report = {
        "input_faces": len(source["indices"]),
        "input_vertices": len(source["positions"]),
        "coupled_vertices": len(positions),
        "output_faces": len(selected),
        "output_vertices": len(compact),
        "requested_target_fraction": target_fraction,
        "requested_absolute_metric_error": absolute_error_m,
        "reported_combined_metric_error": float(error[0]),
        "lineages": len(prepared["lineage"]),
        "output_lineages": len(np.unique(owners)),
        "complete_corner_key_bytes": prepared["key_bytes"],
        "attribute_array_bytes": attributes.nbytes,
        "protected_category_vertices": int(np.count_nonzero(locks)),
        "permissive": permissive,
        "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
        "destination_index_bytes": destination.nbytes,
        "known_output_array_bytes": sum(a.nbytes for a in result.values()),
        "error_scope": "QEM+attribute metric; not a geometric/Hausdorff, alpha, material-light or conservative occlusion certificate",
    }
    return result, report
