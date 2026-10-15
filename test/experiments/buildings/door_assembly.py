"""Assemble static portal frames and selected leaf poses with stable part ownership."""

import numpy as np
from door_frame import frame
from door_mesh import expand, template


def prepare(selected):
    return tuple(
        (row["leaf"], frame(row["leaf"]), template(row["leaf"])) for row in selected
    )


def mesh(selected, angle_deg, *, fixed=None):
    fixed = prepare(selected) if fixed is None else fixed
    if len(fixed) != len(selected) or any(
        cached[0] != row["leaf"] for cached, row in zip(fixed, selected, strict=True)
    ):
        raise ValueError("Prepared door geometry differs from its fixed plan")
    arrays = {
        key: []
        for key in (
            "vertices",
            "faces",
            "materials",
            "parts",
            "levels",
            "opening_indices",
        )
    }
    offset = part_offset = 0
    for index, (row, cached) in enumerate(zip(selected, fixed, strict=True)):
        leaf = row["leaf"]
        for body in (cached[1], expand(leaf, cached[2], angle_deg)):
            arrays["vertices"].append(body["vertices"])
            arrays["faces"].append(body["faces"] + offset)
            arrays["materials"].append(body["materials"])
            arrays["parts"].append(body["parts"] + part_offset)
            arrays["levels"].append(
                np.full(len(body["faces"]), row["storey"], dtype=np.uint8)
            )
            arrays["opening_indices"].append(
                np.full(len(body["faces"]), index, dtype=np.int32)
            )
            offset += len(body["vertices"])
            part_offset += int(body["parts"].max()) + 1
    if not selected:
        return {
            "vertices": np.empty((0, 3)),
            "faces": np.empty((0, 3), dtype=np.int32),
            **{
                key: np.empty(0, dtype=np.int32)
                for key in arrays
                if key not in ("vertices", "faces")
            },
        }
    return {key: np.concatenate(value) for key, value in arrays.items()}
