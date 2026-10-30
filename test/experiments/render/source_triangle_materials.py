"""Feed original triangle ordinals to the existing native material renderer."""

import json

import numpy as np


def triangle_parts(source, ordinals):
    faces = source["indices"]
    selected = np.asarray(ordinals)
    if (
        selected.ndim != 1
        or selected.dtype.kind not in "iu"
        or not 1 <= len(selected) <= 131072
        or selected.min() < 0
        or selected.max() >= len(faces)
    ):
        raise ValueError("Bounded original triangle ordinals required")
    selected = np.unique(selected)
    vertex = first = 0
    result = []
    for part, attributes in zip(source["parts"], source["attributes"], strict=True):
        count = part["vertices"]
        end = first + part["triangles"]
        keep = selected[(selected >= first) & (selected < end)]
        indices = faces[keep].astype(np.int64) - vertex
        if indices.size and (indices.min() < 0 or indices.max() >= count):
            raise ValueError("Original triangles cross their material part")
        result.append(
            (
                {**part, "triangles": len(keep)},
                {
                    "positions": source["positions"][vertex : vertex + count],
                    **attributes,
                    "indices": indices.astype("<u4"),
                },
            )
        )
        vertex += count
        first = end
    if vertex != len(source["positions"]) or first != len(faces):
        raise ValueError("Whole original part inventory required")
    return tuple(result)


def write_source(source, ordinals, directory):
    parts = triangle_parts(source, ordinals)
    geometry = directory / "geometry" / "0"
    geometry.mkdir(parents=True, exist_ok=False)
    for at, (_, values) in enumerate(parts):
        folder = geometry / str(at)
        folder.mkdir()
        for name, value in values.items():
            dtype = "<u4" if name == "indices" else "<f4"
            np.asarray(value, dtype=dtype).tofile(folder / (name + ".bin"))
    (geometry / "input.json").write_text(
        json.dumps({"parts": [part for part, _ in parts]}, indent=2) + "\n"
    )
    return sum(part["triangles"] for part, _ in parts)
