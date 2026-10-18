"""Compare root-before-minimum with minimum-before-root on original class cells."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from ground_class_work import analyse


def expand_ranges(first, counts):
    starts = np.concatenate((np.zeros(1, dtype=np.int64), counts.cumsum()[:-1]))
    return np.repeat(first - starts, counts) + np.arange(int(counts.sum()))


def compare(words, head):
    if not head:
        return None
    width, height = map(int, words[head : head + 2])
    cells_at, seeds_at, refs_at, edges_at = map(int, words[head + 5 : head + 9])
    cells = words[cells_at:seeds_at].reshape(-1, 2)
    counts = ((cells[:, 0] >> 16) & 255).astype(np.int64)
    if counts.sum() > 1000000:
        raise ValueError("complete seed-query expansion exceeds the experiment memory bound")
    owners = np.repeat(np.arange(len(cells)), counts)
    seed_ids = expand_ranges(cells[:, 1].astype(np.int64), counts)
    seeds = words[seeds_at:refs_at].reshape(-1, 4)[seed_ids]
    ref_counts = seeds[:, 2].astype(np.int64)
    if ref_counts.sum() > 2000000:
        raise ValueError("complete segment-query expansion exceeds the experiment memory bound")
    ref_ids = expand_ranges(seeds[:, 1].astype(np.int64), ref_counts)
    edge_at = edges_at + words[refs_at + ref_ids].astype(np.int64) * 4
    if np.any(edge_at + 3 >= len(words)):
        raise ValueError("reachable edge outside captured data")
    floats = words.view("<f4")
    origin, cell_m = floats[head + 2 : head + 4], floats[head + 4]
    cell_xy = np.column_stack((owners % width, owners // width)).astype(np.float32)
    centres = origin + (cell_xy + np.float32(0.5)) * cell_m
    point = np.repeat(centres, ref_counts, axis=0)
    a = np.column_stack((floats[edge_at], floats[edge_at + 1]))
    b = np.column_stack((floats[edge_at + 2], floats[edge_at + 3]))
    delta = b - a
    length2 = np.sum(delta * delta, axis=1, dtype=np.float32)
    t = np.zeros(len(delta), dtype=np.float32)
    np.divide(np.sum((point - a) * delta, axis=1, dtype=np.float32), length2,
              out=t, where=length2 > 0)
    offset = a + np.clip(t, 0, 1)[:, None] * delta - point
    squared = np.sum(offset * offset, axis=1, dtype=np.float32)
    prefix = np.concatenate((np.zeros(1, dtype=np.int64), ref_counts.cumsum()[:-1]))
    nonempty = ref_counts > 0
    original = np.full(len(seeds), np.float32(1e30))
    candidate = original.copy()
    if nonempty.any():
        starts = prefix[nonempty]
        original[nonempty] = np.minimum(np.float32(1e30),
                                       np.minimum.reduceat(np.sqrt(squared), starts))
        candidate[nonempty] = np.minimum(np.float32(1e30),
                                        np.sqrt(np.minimum.reduceat(squared, starts)))
    if not np.array_equal(original.view("u4"), candidate.view("u4")):
        raise ValueError("Float32 distance bits changed")
    return {
        "cells": width * height,
        "seed_queries": len(seeds),
        "segment_visits": len(squared),
        "original_roots": len(squared),
        "candidate_roots_including_empty_seeds": len(seeds),
        "identical_distance_bits": True,
        "distance_sha256": hashlib.sha256(original.tobytes()).hexdigest(),
    }


def candidate_source(source):
    changes = (
        ("float groundSegDist(", "float groundSegDistSquared("),
        ("return length(a + t * d - p);", "vec2 offset = a + t * d - p;\n  return dot(offset, offset);"),
        ("      float d = kGroundNoEdgeM;", "      float dSquared = uintBitsToFloat(0x7f800000u);"),
        ("d = min(d, groundSegDist(at, p0, p1));", "dSquared = min(dSquared, groundSegDistSquared(at, p0, p1));"),
        ("      bool inside =", "      float d = min(kGroundNoEdgeM, sqrt(dSquared));\n      bool inside ="),
    )
    for before, after in changes:
        if source.count(before) != 1:
            raise ValueError("classifier source boundary changed: " + before)
        source = source.replace(before, after)
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    inventory = analyse(args.capture)
    words = np.frombuffer(args.capture.read_bytes(), dtype="<u4")
    print(json.dumps({"input_sha256": inventory["sha256"],
                      "fine": compare(words, int(words[0])),
                      "coarse": compare(words, int(words[1])),
                      "scope": "One Float32 cell-centre query per cell, original reachable seeds and edges. CPU arithmetic only; not pixel weights, Metal/Vulkan math or a speedup claim."}, indent=2))


if __name__ == "__main__":
    main()
