"""Feed original triangle ordinals to the existing native material renderer."""

import json

import numpy as np


def triangle_parts(source, ordinals, *, referenced_vertices=False):
    faces = source["indices"]
    selected = np.asarray(ordinals)
    if (
        selected.ndim != 1
        or selected.dtype.kind not in "iu"
        or len(selected) > 131072
        or (len(selected) and (selected.min() < 0 or selected.max() >= len(faces)))
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
        values = {
            "positions": source["positions"][vertex : vertex + count],
            **attributes,
        }
        kept_vertices = count
        if referenced_vertices:
            used, remapped = np.unique(indices, return_inverse=True)
            if any(
                value.size and (value.ndim != 2 or len(value) != count)
                for value in values.values()
            ):
                raise ValueError(
                    "Whole per-vertex attribute rows required before compaction"
                )
            values = {
                name: value[used] if len(value) else value
                for name, value in values.items()
            }
            indices = remapped.reshape(indices.shape)
            kept_vertices = len(used)
        result.append(
            (
                {**part, "triangles": len(keep), "vertices": kept_vertices},
                {**values, "indices": indices.astype("<u4")},
            )
        )
        vertex += count
        first = end
    if vertex != len(source["positions"]) or first != len(faces):
        raise ValueError("Whole original part inventory required")
    return tuple(result)


def write_source(source, ordinals, directory, *, referenced_vertices=False):
    parts = triangle_parts(source, ordinals, referenced_vertices=referenced_vertices)
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
