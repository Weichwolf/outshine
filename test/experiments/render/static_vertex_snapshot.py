"""Measure canonical-vertex model admission from a native geometry snapshot."""

import argparse
import json
from pathlib import Path

from resident_vertex_references import load
from static_vertex_batches import Draw, batch


def measure(metadata, streams):
    roots, templates, batches = (streams[name] for name in ("roots", "templates", "batches"))
    source = streams["source_indices"].reshape(-1)
    grouped_roots = [[] for _ in batches]
    for root in roots:
        if int(root[7]) >= len(batches):
            raise ValueError("root names a missing batch")
        if not int(root[11]) & 4:
            grouped_roots[int(root[7])].append(root)
    draws = []
    keys = {}
    fallback = 0
    for row, members in zip(batches, grouped_roots):
        if not members:
            continue
        models = {int(root[8]) for root in members}
        if int(row[5]) != 1 or len(models) != 1:
            fallback += 1
            continue
        least, most = None, 0
        for root in members:
            first, count, base = int(root[4]), int(root[5]), int(root[9])
            if first + count > len(templates):
                raise ValueError("root exceeds source templates")
            for template in templates[first:first + count]:
                start, size = base + int(template[1]), int(template[2])
                if start + size > len(source) or size % 3:
                    raise ValueError("invalid source index range")
                if size:
                    indices = source[start:start + size]
                    low, high = int(indices.min()), int(indices.max()) + 1
                    least = low if least is None else min(least, low)
                    most = max(most, high)
        if least is None:
            continue
        key = tuple(map(int, row[[0, 1, 2, 3, 4, 11]]))
        draws.append(Draw(least, most - least, models.pop(), keys.setdefault(key, len(keys))))
    vertices = max((draw.first + draw.count for draw in draws), default=0)
    models, groups, rejected = batch(vertices, draws)
    return {"scope": metadata["scope"], "generation": metadata["generation"],
            "candidate_draws": len(draws), "compatible_groups": len(groups),
            "merged_draws": sum(map(len, groups.values())),
            "fallback_draws": fallback + len(rejected),
            "vertex_prefix": vertices, "model_table_bytes": models.nbytes,
            "note": "metadata admission only; no GPU speedup, PBR, motion or pass proof"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(measure(*load(args.directory)), indent=2))
