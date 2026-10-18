"""Share identical window surfaces while retaining every light's blocker domain."""

import numpy as np


def role_packets(selected, indices):
    if not selected or next(iter(selected)) != "camera" or len(selected) > 8:
        raise ValueError(
            "camera followed by at most seven named light queries required"
        )
    names, arrays = tuple(selected), [np.asarray(v) for v in selected.values()]
    size = len(arrays[0])
    if size > 80000:
        raise ValueError("complete view selection exceeds the window budget")
    if any(
        v.shape != (size,) or v.dtype.kind not in "iu" or np.any((v < 0) | (v >= 64))
        for v in arrays
    ):
        raise ValueError("complete equal finite64-view selections required")
    indices = np.asarray(indices)
    if (
        indices.ndim != 1
        or indices.dtype.kind not in "iu"
        or len(indices) > 80000
        or np.any(indices < 0)
        or np.any(indices >= size)
        or (len(indices) > 1 and np.any(indices[1:] <= indices[:-1]))
    ):
        raise ValueError("bounded distinct ordered physical window indices required")
    local = [v[indices] for v in arrays]
    previous, packets = [], []
    for order, (name, view) in enumerate(zip(names, local, strict=True)):
        keep = np.ones(len(indices), bool)
        for earlier in previous:
            keep &= view != earlier
        owners = np.zeros(len(indices), np.uint8)
        for bit, light_view in enumerate(local[1:]):
            owners |= (view == light_view).astype(np.uint8) << bit
        for code in np.unique(owners[keep]):
            members = indices[keep & (owners == code)]
            lights = tuple(
                n for bit, n in enumerate(names[1:]) if int(code) & (1 << bit)
            )
            material = ("all" if lights else "camera") if order == 0 else "shadow"
            immutable = np.frombuffer(
                members.astype(np.int64).tobytes(), dtype=np.int64
            )
            packets.append((name, material, lights, immutable))
        previous.append(view)
    return tuple(packets)
