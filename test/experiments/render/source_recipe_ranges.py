"""Read selected prepared recipes from indexed, integrity-bound byte ranges."""

import hashlib

import numpy as np
from prepared_surface_recipes import Reader, read_surface


def load_ranges(table, hashes, metadata, selected, gap=0):
    if (
        table.dtype != np.dtype("<u4")
        or table.ndim != 2
        or table.shape[1] != 4
        or hashes.dtype != np.uint8
        or hashes.shape != (len(table), 32)
        or type(gap) is not int
        or not 0 <= gap <= 4096
    ):
        raise ValueError("Bounded source recipe span index required")
    membership = {
        int(row[0]): (int(row[1]), int(row[2]), int(row[3]), hashes[k].tobytes())
        for k, row in enumerate(table)
    }
    if len(membership) != len(table) or not selected.issubset(membership):
        raise ValueError("Unique complete requested source recipes required")
    paths = {cell: path for cell, path in metadata}
    if len(paths) != len(metadata):
        raise ValueError("Unique prepared recipe cells required")
    requested = sorted(
        (membership[i][0], membership[i][1], membership[i][2], i, membership[i][3])
        for i in selected
    )
    runs = []
    for record in requested:
        cell, offset, length, _, _ = record
        if not length or cell not in paths:
            raise ValueError("Known nonempty recipe ranges required")
        if runs and runs[-1][0] == cell and offset <= runs[-1][2] + gap:
            runs[-1][2] = max(runs[-1][2], offset + length)
            runs[-1][3].append(record)
        else:
            runs.append([cell, offset, offset + length, [record]])
    recipes, total_bytes, scalars = {}, 0, 0
    for cell, start, end, records in runs:
        with paths[cell].open("rb") as stream:
            stream.seek(start)
            blob = stream.read(end - start)
        if len(blob) != end - start:
            raise ValueError("Incomplete recipe range; retain original")
        total_bytes += len(blob)
        for _, offset, length, identity, expected in records:
            raw = blob[offset - start : offset - start + length]
            if hashlib.sha256(raw).digest() != expected:
                raise ValueError("Changed source recipe; retain original")
            reader = Reader(raw)
            value = read_surface(reader)
            if reader.at != len(raw) or value is None:
                raise ValueError("Complete supported source recipe required")
            scalars += reader.numbers
            recipes[identity] = value
    return recipes, {
        "recipe_range_bytes": total_bytes,
        "recipe_range_reads": len(runs),
        "recipe_scalar_reads": scalars,
        "recipe_sources_decoded": len(recipes),
    }
