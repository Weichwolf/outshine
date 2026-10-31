"""Restore the existing native material-part inputs from complete original rows."""

import json

import numpy as np
from source_original_ranges import ROW

FIELDS = ("positions", "normals", "uv", "uv1", "colours")
SLICES = (slice(0, 3), slice(3, 6), slice(6, 8), slice(8, 10), slice(10, 14))


def part_inputs(rows, original_parts):
    if rows.dtype != ROW or rows.ndim != 1 or not original_parts:
        raise ValueError("Bound original rows and material parts required")
    if len(rows) > 1 and np.any(rows["ordinal"][1:] <= rows["ordinal"][:-1]):
        raise ValueError("Original triangle order must be unique and increasing")
    first_triangle = first_vertex = 0
    result = []
    for part in original_parts:
        triangles, vertices = part["triangles"], part["vertices"]
        if (
            type(triangles) is not int
            or type(vertices) is not int
            or min(triangles, vertices) < 0
        ):
            raise ValueError("Bounded original part sizes required")
        end_triangle, end_vertex = first_triangle + triangles, first_vertex + vertices
        selected = rows[
            (rows["ordinal"] >= first_triangle) & (rows["ordinal"] < end_triangle)
        ]
        ids = selected["indices"].reshape(-1)
        if ids.size and (ids.min() < first_vertex or ids.max() >= end_vertex):
            raise ValueError("Original triangle crosses its material part")
        used, first, inverse = np.unique(ids, return_index=True, return_inverse=True)
        words = np.ascontiguousarray(selected["corners"]).reshape(-1, 14).view("<u4")
        if not np.array_equal(words, words[first][inverse]):
            raise ValueError("Original vertex words disagree across corners")
        compact = words[first].view("<f4")
        arrays = {
            name: compact[:, columns]
            for name, columns in zip(FIELDS, SLICES, strict=True)
        }
        arrays["indices"] = inverse.reshape(-1, 3).astype("<u4")
        result.append(
            (
                {**part, "vertices": len(used), "triangles": len(selected)},
                arrays,
                selected["binding"].copy(),
            )
        )
        first_triangle, first_vertex = end_triangle, end_vertex
    if len(rows) and int(rows["ordinal"][-1]) >= first_triangle:
        raise ValueError("Original triangle lies outside the part inventory")
    return tuple(result)


def write_part_inputs(parts, directory):
    geometry = directory / "geometry" / "0"
    geometry.mkdir(parents=True, exist_ok=False)
    for number, (_, arrays, _) in enumerate(parts):
        folder = geometry / str(number)
        folder.mkdir()
        for name, value in arrays.items():
            np.asarray(value, dtype="<u4" if name == "indices" else "<f4").tofile(
                folder / (name + ".bin")
            )
    (geometry / "input.json").write_text(
        json.dumps({"parts": [part for part, _, _ in parts]}, indent=2) + "\n"
    )
