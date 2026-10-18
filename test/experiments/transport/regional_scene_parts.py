"""Bounded draw parts retain original owner, vertex and face provenance."""

import numpy as np


def parts(data):
    required = {"vertices", "triangles", "normals", "materials"}
    if not required <= data.keys() or not data.keys() <= required | {
        "face_base_colour_linear_rgba"
    }:
        raise ValueError(
            "explicit known per-vertex and per-face source arrays required"
        )
    points, faces = data["vertices"], data["triangles"]
    pending = [(0, len(faces))]
    emitted = 0
    while pending:
        start, stop = pending.pop()
        if stop - start > 65536 and len(points) > 65536:
            pending.extend(
                [(min(start + 65536, stop), stop), (start, min(start + 65536, stop))]
            )
            continue
        if start == stop:
            continue
        selected, inverse = np.unique(faces[start:stop], return_inverse=True)
        if len(selected) > 65536:
            middle = (start + stop) // 2
            pending.extend([(middle, stop), (start, middle)])
            continue
        emitted += 1
        if emitted > 64:
            raise ValueError("complete owner exceeds bounded64-part research draw")
        chunk = {
            "vertices": points[selected],
            "triangles": inverse.reshape(-1, 3),
            "normals": data["normals"].reshape(-1, 3, 3)[start:stop].reshape(-1, 3),
            "materials": data["materials"][start:stop],
            "source_vertex_ids": selected,
            "source_face_ids": np.arange(start, stop, dtype="i8"),
        }
        if "face_base_colour_linear_rgba" in data:
            chunk["face_base_colour_linear_rgba"] = data[
                "face_base_colour_linear_rgba"
            ][start:stop]
        yield chunk
