"""Remove redundant ready-product records; consume wall edges without expansion."""

import itertools

import numpy as np


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
    result["material_bays"] = arrays["material_bays"][wall].copy()
    result["material_edge_offsets"] = np.r_[0, np.cumsum(counts)].astype(np.uint32)
    result["prism_bounds"] = np.empty((0, 2, 3), np.float64)
    result["prism_roof"] = np.empty(0, bool)
    binding = result.pop("fallback_binding")
    result["fallback_source_id"] = binding[:, 1].copy()
    result["fallback_part_id"] = (
        binding[:, 2].copy() if np.any(binding[:, 2] != -1) else np.empty(0, np.int32)
    )
    return result
