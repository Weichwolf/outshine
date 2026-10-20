"""Conservative projected bounding rectangles; variable lists, no depth-layer cap."""

import numpy as np


def build(footprints, pitch=1.0):
    if not np.isfinite(pitch) or pitch <= 0:
        raise ValueError("positive projected-bin pitch required")
    low = np.floor(np.nextafter(footprints.min(axis=(0, 1)) / pitch, -np.inf))
    high = np.floor(np.nextafter(footprints.max(axis=(0, 1)) / pitch, np.inf))
    shape = (high - low + 1).astype(np.int64)
    if np.any(shape <= 0) or np.prod(shape) > 262144:
        raise ValueError("projected field needs a finer spatial node")
    a = np.floor(np.nextafter(footprints.min(1) / pitch, -np.inf)).astype(
        np.int64
    ) - low.astype(np.int64)
    b = np.floor(np.nextafter(footprints.max(1) / pitch, np.inf)).astype(
        np.int64
    ) - low.astype(np.int64)
    references = int(np.prod(b - a + 1, axis=1).sum())
    if references > 4194304:
        raise ValueError("projected field membership budget exceeded")
    bins = [[] for _ in range(int(np.prod(shape)))]
    for face, (start, stop) in enumerate(zip(a, b, strict=True)):
        for y in range(start[1], stop[1] + 1):
            for x in range(start[0], stop[0] + 1):
                bins[y * shape[0] + x].append(face)
    offsets = np.r_[0, np.cumsum([len(cell) for cell in bins])].astype(np.uint32)
    ids = np.array(
        [face for cell in bins for face in cell],
        np.uint16 if len(footprints) <= 65536 else np.uint32,
    )
    return {
        "bin_low": low * pitch,
        "bin_shape": shape.astype(np.uint32),
        "bin_pitch": np.array(pitch),
        "bin_offsets": offsets,
        "bin_faces": ids,
    }


def pairs(arrays, first, last, active):
    """A projected ray segment lies inside its rectangle; overestimate is explicit."""
    shape = arrays["bin_shape"].astype(np.int64)
    low, pitch = arrays["bin_low"], float(arrays["bin_pitch"])
    a = np.floor(np.nextafter((np.minimum(first, last) - low) / pitch, -np.inf))
    b = np.floor(np.nextafter((np.maximum(first, last) - low) / pitch, np.inf))
    ray_ids, face_ids, visits, raw = [], [], 0, 0
    for ray in np.flatnonzero(active & (b >= 0).all(1) & (a < shape).all(1)):
        start = np.maximum(a[ray], 0).astype(np.int64)
        stop = np.minimum(b[ray], shape - 1).astype(np.int64)
        chunks = []
        for y in range(start[1], stop[1] + 1):
            for x in range(start[0], stop[0] + 1):
                cell = y * shape[0] + x
                begin, end = arrays["bin_offsets"][cell : cell + 2]
                chunks.append(arrays["bin_faces"][begin:end])
                visits += 1
        ids = np.unique(np.concatenate(chunks))
        raw += sum(len(chunk) for chunk in chunks)
        if raw > 1048576:
            raise ValueError("projected query batch exceeds field-work budget")
        ray_ids.extend([ray] * len(ids))
        face_ids.extend(ids)
    return (
        np.asarray(ray_ids, np.int64),
        np.asarray(face_ids, np.int64),
        {"bin_visits": visits, "program_references_before_dedup": raw},
    )
