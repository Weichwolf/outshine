"""Bounded same-parent stream packing; no native draw/ABI or visibility decision."""

import numpy as np
from cached_building_streams import STREAM_BYTES, validate_streams


def pages(products, capacity):
    if type(capacity) is not int or not 4096 <= capacity <= 64 * 1024**2:
        raise ValueError("explicit bounded resident page capacity required")
    contexts = {
        (p["parent"], p["view"]["input_bytes"] if p["view"] else None, p["detail"])
        for p in products
    }
    if len(contexts) > 1:
        raise ValueError("one explicit parent/frame/view/detail cohort required")
    cells = [p["cell"] for p in products]
    if any(c is None for c in cells) or len(set(cells)) != len(cells):
        raise ValueError("unambiguous source cell identities required")
    ordered = sorted(
        products, key=lambda p: (bool(p["counts"][5]), bool(p["counts"][6]), p["cell"])
    )
    result, current, used, signature = [], [], 0, None
    for item in ordered:
        own = (bool(item["counts"][5]), bool(item["counts"][6]))
        size = sum(len(s) for s in item["streams"])
        if size > capacity:
            raise ValueError(
                "source cell exceeds page cap; no oversized allocation attempted"
            )
        if current and (used + size > capacity or own != signature):
            result.append(current)
            current, used = [], 0
        current.append(item)
        used += size
        signature = own
    if current:
        result.append(current)
    return result


def pack(items):
    totals = [sum(item["counts"][i] for item in items) for i in range(10)]
    if max(totals, default=0) > np.iinfo(np.uint32).max:
        raise ValueError("uint32 stream offset overflow")
    into = [
        bytearray(count * width)
        for count, width in zip(totals, STREAM_BYTES, strict=True)
    ]
    offsets, mappings = [0] * 10, []
    for item in items:
        for i, (source, width) in enumerate(
            zip(item["streams"], STREAM_BYTES, strict=True)
        ):
            first = offsets[i] * width
            into[i][first : first + len(source)] = source
            if i in (2, 3, 7, 9):
                vertices = 0 if i in (2, 7) else 1
                np.frombuffer(
                    into[i], dtype="<u4", count=item["counts"][i], offset=first
                )[:] += offsets[vertices]
            if i in (4, 8):
                indices = 7 if i == 4 else 9
                records = np.frombuffer(
                    into[i], dtype="<u4", count=item["counts"][i] * 13, offset=first
                ).reshape(-1, 13)
                records[:, 10] += offsets[indices]
        mappings.append(
            {
                "key": item["key"],
                "cell": item["cell"],
                "offsets": offsets.copy(),
                "counts": item["counts"],
            }
        )
        offsets = [a + b for a, b in zip(offsets, item["counts"], strict=True)]
    validate_streams(into, totals)
    return into, mappings


def verify(packed, mappings, originals):
    """Recover each source independently, including every DAG level and attribute byte."""
    by_key = {p["key"]: p for p in originals}
    if len(by_key) != len(originals) or len(mappings) != len(originals):
        raise ValueError("duplicate or omitted source cell")
    for i, width in enumerate(STREAM_BYTES):
        spans = sorted(
            (row["offsets"][i] * width, (row["offsets"][i] + row["counts"][i]) * width)
            for row in mappings
            if row["counts"][i]
        )
        cursor = 0
        for first, end in spans:
            if first != cursor:
                raise ValueError("source ranges alias or leave a storage hole")
            cursor = end
        if cursor != len(packed[i]):
            raise ValueError("source ranges do not cover the resident stream")
    for row in mappings:
        original = by_key.pop(row["key"], None)
        if (
            original is None
            or row["cell"] != original["cell"]
            or tuple(row["counts"]) != original["counts"]
        ):
            raise ValueError("source identity or extent changed")
        for i, width in enumerate(STREAM_BYTES):
            start, count = row["offsets"][i] * width, row["counts"][i]
            restored = bytearray(packed[i][start : start + count * width])
            if i in (2, 3, 7, 9):
                vertex_stream = 0 if i in (2, 7) else 1
                np.frombuffer(restored, dtype="<u4")[:] -= row["offsets"][vertex_stream]
            if i in (4, 8):
                index_stream = 7 if i == 4 else 9
                records = np.frombuffer(restored, dtype="<u4").reshape(-1, 13)
                records[:, 10] -= row["offsets"][index_stream]
            if restored != original["streams"][i]:
                raise ValueError("packed source bytes or indices changed")
    if by_key:
        raise ValueError("source cells omitted")
