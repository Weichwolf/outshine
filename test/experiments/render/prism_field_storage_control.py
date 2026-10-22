"""Exact original post-vertex fields with equal source identity, not extra face tags."""

import numpy as np
from compare_projective_field import factorized_control


def control(source):
    absent = {
        key: np.empty(0, np.uint32)
        for key in ["lineage", "lineage_index", "original_face", "native_face_tag"]
    }
    arrays, _ = factorized_control(source, absent)
    arrays = {key: value for key, value in arrays.items() if key not in absent}
    owner = source["binding"][:, 1].astype(np.int32)
    starts = np.r_[0, np.flatnonzero(owner[1:] != owner[:-1]) + 1].astype(np.uint32)
    arrays["source_offsets"] = np.r_[starts, np.uint32(len(owner))]
    arrays["source_ids"] = owner[starts]
    rebuilt = np.repeat(arrays["source_ids"], np.diff(arrays["source_offsets"]))
    np.testing.assert_array_equal(rebuilt, owner)
    return arrays, {
        "array_bytes": {key: value.nbytes for key, value in arrays.items()},
        "total_array_bytes": sum(value.nbytes for value in arrays.values()),
        "source_runs": len(starts),
        "scope": "Exact position, post-vertex coordinates/metres/RGBA, flat codes and packed normals at every original triangle corner; complete source ordinals. Original triangle order retained, native part/face tags not required or invented. Storage control, not native layout/GPU timing.",
    }
