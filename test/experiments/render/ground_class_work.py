"""Count packed classifier list work per cell; not pixel coverage or GPU time."""

import hashlib
import re
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]


def seed_stride():
    source = ROOT / "src/base/spatial/ClassSeedValues.h"
    return int(re.search(r"kClassSeedWordCount\s*=\s*(\d+)", source.read_text())[1])


def distribution(values):
    values = np.asarray(values)
    if not values.size:
        return {"cells": 0, "total": 0, "mean": None, "max": None, "bins": {}}
    bins = {}
    lower = -1
    for upper in (0, 4, 16, 64, 256):
        bins[f"{lower + 1}..{upper}"] = int(
            np.count_nonzero((values > lower) & (values <= upper))
        )
        lower = upper
    bins[">256"] = int(np.count_nonzero(values > lower))
    return {
        "cells": int(values.size),
        "total": int(values.sum(dtype=np.uint64)),
        "mean": float(values.mean()),
        "max": int(values.max()),
        "bins": bins,
    }


def grid_work(words, head, stride):
    if head == 0:
        return None
    if head < 4 or head + 12 > words.size:
        raise ValueError("grid header outside packed data")
    floats = words.view("<f4")
    if (
        not np.isfinite(floats[head + 2 : head + 5]).all()
        or floats[head + 4] <= 0
        or not np.isfinite(floats[head + 9])
        or floats[head + 9] < 0
    ):
        raise ValueError("invalid spatial grid metadata")
    width, height = map(int, words[head : head + 2])
    cells_at, seeds_at, refs_at, edges_at = map(int, words[head + 5 : head + 9])
    if not (head + 12 <= cells_at <= seeds_at <= refs_at <= edges_at <= words.size):
        raise ValueError("grid sections outside packed data or out of order")
    if seeds_at - cells_at != 2 * width * height or (refs_at - seeds_at) % stride:
        raise ValueError("grid section size differs from its declared layout")
    grid = words[cells_at:seeds_at].reshape(-1, 2)
    seeds = words[seeds_at:refs_at].reshape(-1, stride)
    counts = ((grid[:, 0] >> 16) & 255).astype(np.uint64)
    active = counts > 0
    first = grid[active, 1].astype(np.uint64)
    end = first + counts[active]
    if np.any(end > len(seeds)):
        raise ValueError("cell points outside its seed table")
    invalid = seeds[:, 1].astype(np.uint64) + seeds[:, 2] > edges_at - refs_at
    invalid_prefix = np.concatenate((np.zeros(1, dtype=np.uint64), np.cumsum(invalid, dtype=np.uint64)))
    if np.any(invalid_prefix[end] != invalid_prefix[first]):
        raise ValueError("reachable seed points outside its edge-reference table")
    prefix = np.concatenate((np.zeros(1, dtype=np.uint64), np.cumsum(seeds[:, 2], dtype=np.uint64)))
    references = np.zeros(len(grid), dtype=np.uint64)
    references[active] = prefix[end] - prefix[first]
    return {
        "width": width,
        "height": height,
        "origin_m": [float(v) for v in floats[head + 2 : head + 4]],
        "cell_m": float(floats[head + 4]),
        "halo_m": float(floats[head + 9]),
        "direct_class_cells": int(np.count_nonzero((grid[:, 0] & 255) != 255)),
        "seed_records": len(seeds),
        "edge_references": edges_at - refs_at,
        "seeds_per_cell": distribution(counts),
        "reference_visits_per_cell": distribution(references),
    }


def analyse(path):
    data = Path(path).read_bytes()
    if len(data) < 16 or len(data) % 4:
        raise ValueError("incomplete packed uint32 classifier")
    words = np.frombuffer(data, dtype="<u4")
    stride = seed_stride()
    if stride != 4:
        raise ValueError("classifier seed layout changed; adapt the reader explicitly")
    return {
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "fine": grid_work(words, int(words[0]), stride),
        "coarse": grid_work(words, int(words[1]), stride),
        "scope": "List visits for a queried packed cell, independently per tier. No pixel weights, visibility, fragment invocations or GPU-time attribution. Fine-footprint skips and fine/coarse precedence remain outside this inventory.",
    }
