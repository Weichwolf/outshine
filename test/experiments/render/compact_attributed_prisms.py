"""Remove redundant ready-product records; consume wall edges without expansion."""

import itertools

import numpy as np


def narrow_integer(values):
    values = np.asarray(values)
    low, high = int(values.min(initial=0)), int(values.max(initial=0))
    candidates = (
        (np.uint8, np.uint16, np.uint32) if low >= 0 else (np.int8, np.int16, np.int32)
    )
    dtype = next(
        (
            dtype
            for dtype in candidates
            if np.iinfo(dtype).min <= low and high <= np.iinfo(dtype).max
        ),
        None,
    )
    if dtype is None:
        raise ValueError("Bounded integer field cannot use the supported table widths")
    return values.astype(dtype)


def compact(arrays):
    result = dict(arrays)
    wall = arrays["material_codes"][:, 0] >= 0
    offsets = arrays["prism_offsets"]
    counts = [
        np.count_nonzero(wall[begin:end]) for begin, end in itertools.pairwise(offsets)
    ]
    for row, (begin, end) in enumerate(itertools.pairwise(offsets)):
        expected = np.zeros(int(end - begin), bool)
        expected[1 : 1 + counts[row]] = True
        if not np.array_equal(wall[begin:end], expected):
            raise ValueError("Own primitive wall-plane ordering is required")
    result["material_edge"] = arrays["material_edge"][wall].copy()
    result["material_bays"] = narrow_integer(arrays["material_bays"][wall])
    result["material_edge_offsets"] = np.r_[0, np.cumsum(counts)].astype(np.uint32)
    result["prism_bounds"] = np.empty((0, 2, 3), np.float64)
    result["prism_roof"] = np.empty(0, bool)
    binding = result.pop("fallback_binding")
    result["fallback_source_id"] = binding[:, 1].copy()
    result["fallback_part_id"] = (
        binding[:, 2].copy() if np.any(binding[:, 2] != -1) else np.empty(0, np.int32)
    )
    codes, lookup = np.unique(arrays["material_codes"], axis=0, return_inverse=True)
    np.testing.assert_array_equal(codes[lookup], arrays["material_codes"])
    result["material_codes"] = narrow_integer(codes)
    result["material_code_index"] = narrow_integer(lookup)
    for key in [
        "prism_offsets",
        "prism_binding",
        "prism_fallback_faces",
        "node_children",
        "material_edge_offsets",
        "fallback_indices",
        "fallback_source_id",
        "fallback_part_id",
    ]:
        result[key] = narrow_integer(result[key])
    return result
