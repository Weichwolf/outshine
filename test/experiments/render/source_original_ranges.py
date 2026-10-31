"""Experimental exact source ranges for the recorded original triangle cohort.

Source ordinals are the bound capture's ordinals, not live World identities.
"""

import hashlib

import numpy as np

ROW = np.dtype(
    [
        ("ordinal", "<u4"),
        ("indices", "<u4", (3,)),
        ("binding", "<i4", (6,)),
        ("corners", "<f4", (3, 14)),
    ]
)
INDEX = np.dtype(
    [("source", "<i4"), ("offset", "<u8"), ("bytes", "<u4"), ("digest", "u1", (32,))]
)
FIELDS = ("normals", "uv", "uv1", "colours")


def digest(raw):
    return hashlib.sha256(raw).digest()


def original_rows(source, binding):
    faces = source["indices"]
    if (
        binding.dtype != np.dtype("<i4")
        or binding.shape != (len(faces), 6)
        or faces.ndim != 2
        or faces.shape[1] != 3
        or faces.dtype.kind not in "iu"
        or len(faces) >= 2**32
        or (faces.size and (faces.min() < 0 or faces.max() >= len(source["positions"])))
    ):
        raise ValueError("Complete bounded original triangle/source binding required")
    arrays = [source["positions"]]
    arrays.extend(
        np.concatenate([part[key] for part in source["attributes"]]) for key in FIELDS
    )
    if any(
        a.dtype != np.dtype("<f4") or len(a) != len(source["positions"]) for a in arrays
    ):
        raise ValueError("Complete original vertex attributes required")
    vertices = np.concatenate(arrays, axis=1)
    if vertices.shape != (len(source["positions"]), 14):
        raise ValueError("Original position and eleven attribute words required")
    rows = np.empty(len(faces), dtype=ROW)
    rows["ordinal"] = np.arange(len(faces), dtype="<u4")
    rows["indices"] = faces
    rows["binding"] = binding
    rows["corners"] = vertices[faces]
    return rows


def write_ranges(directory, rows):
    if rows.dtype != ROW or rows.ndim != 1 or not len(rows):
        raise ValueError("Bounded original rows required")
    order = np.argsort(rows["binding"][:, 1], kind="stable")
    ordered = rows[order]
    ids, starts, counts = np.unique(
        ordered["binding"][:, 1], return_index=True, return_counts=True
    )
    index = np.empty(len(ids), dtype=INDEX)
    directory.mkdir(parents=True, exist_ok=False)
    with (directory / "rows.bin").open("wb") as stream:
        for i, (identity, start, count) in enumerate(
            zip(ids, starts, counts, strict=True)
        ):
            raw = ordered[start : start + count].tobytes()
            index[i] = (
                identity,
                stream.tell(),
                len(raw),
                np.frombuffer(digest(raw), "u1"),
            )
            stream.write(raw)
    np.save(directory / "index.npy", index, allow_pickle=False)
    return index


def read_ranges(directory, index, requested):
    if index.dtype != INDEX or index.ndim != 1:
        raise ValueError("Exact original source range index required")
    ids = [int(x) for x in requested]
    if len(ids) != len(set(ids)):
        raise ValueError("Distinct requested source IDs required")
    lookup = {int(row["source"]): row for row in index}
    if len(lookup) != len(index) or not set(ids).issubset(lookup):
        raise ValueError("Unknown or duplicate source; retain full original")
    records = sorted((lookup[i] for i in ids), key=lambda r: int(r["offset"]))
    spans = []
    for row in records:
        start, length = int(row["offset"]), int(row["bytes"])
        if length <= 0 or length % ROW.itemsize or (spans and start < spans[-1][1]):
            raise ValueError("Invalid or overlapping original source range")
        if spans and start == spans[-1][1]:
            spans[-1][1] += length
            spans[-1][2].append(row)
        else:
            spans.append([start, start + length, [row]])
    results, read_bytes = [], 0
    with (directory / "rows.bin").open("rb") as stream:
        for start, end, group in spans:
            stream.seek(start)
            blob = stream.read(end - start)
            if len(blob) != end - start:
                raise ValueError("Incomplete original source range")
            read_bytes += len(blob)
            for row in group:
                lo = int(row["offset"]) - start
                raw = blob[lo : lo + int(row["bytes"])]
                if digest(raw) != row["digest"].tobytes():
                    raise ValueError("Original source range changed")
                values = np.frombuffer(raw, dtype=ROW)
                if np.any(values["binding"][:, 1] != row["source"]):
                    raise ValueError("Original source identity changed")
                results.append(values)
    output = np.concatenate(results) if results else np.empty(0, dtype=ROW)
    output.sort(order="ordinal")
    if len(np.unique(output["ordinal"])) != len(output):
        raise ValueError("Duplicate original triangles")
    return output, {"read_calls": len(spans), "read_bytes": read_bytes}
