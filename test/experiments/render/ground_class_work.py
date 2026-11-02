"""Count packed classifier list work per cell; not pixel coverage or GPU time."""

import array
import hashlib
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def seed_stride():
    source = ROOT / "src/base/spatial/ClassSeedValues.h"
    return int(re.search(r"kClassSeedWordCount\s*=\s*(\d+)", source.read_text())[1])


def distribution(values):
    if len(values) == 0:
        return {"cells": 0, "total": 0, "mean": None, "max": None, "bins": {}}
    bins = {}
    lower = -1
    for upper in (0, 4, 16, 64, 256):
        bins[f"{lower + 1}..{upper}"] = sum(lower < value <= upper for value in values)
        lower = upper
    bins[">256"] = sum(value > lower for value in values)
    return {
        "cells": len(values),
        "total": sum(values),
        "mean": sum(values) / len(values),
        "max": max(values),
        "bins": bins,
    }


def grid_work(words, head, stride):
    if head == 0:
        return None
    if head < 4 or head + 12 > len(words):
        raise ValueError("grid header outside packed data")

    def as_float(word):
        return struct.unpack("<f", struct.pack("<I", word))[0]

    origin = [as_float(word) for word in words[head + 2 : head + 4]]
    cell_m, halo_m = as_float(words[head + 4]), as_float(words[head + 9])
    if (
        not all(math.isfinite(v) for v in [*origin, cell_m, halo_m])
        or cell_m <= 0
        or halo_m < 0
    ):
        raise ValueError("invalid spatial grid metadata")
    width, height = words[head : head + 2]
    cells_at, seeds_at, refs_at, edges_at = words[head + 5 : head + 9]
    if not (head + 12 <= cells_at <= seeds_at <= refs_at <= edges_at <= len(words)):
        raise ValueError("grid sections outside packed data or out of order")
    if seeds_at - cells_at != 2 * width * height or (refs_at - seeds_at) % stride:
        raise ValueError("grid section size differs from its declared layout")
    counts, references = [], []
    direct = 0
    seed_count = (refs_at - seeds_at) // stride
    for cell in range(cells_at, seeds_at, 2):
        count = (words[cell] >> 16) & 255
        first = words[cell + 1]
        if count and first + count > seed_count:
            raise ValueError("cell points outside its seed table")
        visits = 0
        for seed in range(first, first + count):
            at = seeds_at + seed * stride
            if words[at + 1] + words[at + 2] > edges_at - refs_at:
                raise ValueError(
                    "reachable seed points outside its edge-reference table"
                )
            visits += words[at + 2]
        counts.append(count)
        references.append(visits)
        direct += (words[cell] & 255) != 255
    return {
        "width": width,
        "height": height,
        "origin_m": origin,
        "cell_m": cell_m,
        "halo_m": halo_m,
        "direct_class_cells": direct,
        "seed_records": seed_count,
        "edge_references": edges_at - refs_at,
        "seeds_per_cell": distribution(counts),
        "reference_visits_per_cell": distribution(references),
    }


def analyse(path):
    data = Path(path).read_bytes()
    if len(data) < 16 or len(data) % 4:
        raise ValueError("incomplete packed uint32 classifier")
    words = array.array("I")
    words.frombytes(data)
    if sys.byteorder != "little":
        words.byteswap()
    stride = seed_stride()
    if words[3] != 0 or stride != 4:
        raise ValueError("classifier layout changed; adapt the reader explicitly")
    return {
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "fine": grid_work(words, words[0], stride),
        "coarse": grid_work(words, words[1], stride),
        "scope": "List visits per queried cell, independently per tier. No pixel weights, visibility, fragment invocations or GPU-time attribution. Footprint skips and tier precedence are not counted.",
    }
