"""Acquisition-time bounded quadtree pages for source-envelope metadata.

Membership is the declared finite source snapshot, not all Earth cells. Spatial
prefixes organize ownership; bounds still enclose actual course overhangs.
"""

import dataclasses
import hashlib
import json
from dataclasses import dataclass

import numpy as np
from outshine_ref.places.sources import tile_tuple
from water_course_envelope import CourseSummary


@dataclass(frozen=True)
class MetadataRef:
    bound: CourseSummary
    source_cells: int
    leaf: bool


def aggregate(key, children):
    children = tuple(sorted(children, key=lambda c: c.bound.key))
    if not 2 <= len(children) <= 4:
        raise ValueError("compressed quadtree pages need two to four children")
    quadrants = []
    for child in children:
        z, x, y = tile_tuple(child.bound.key)
        dz = z - key[0]
        if dz <= 0 or (x >> dz, y >> dz) != key[1:]:
            raise ValueError("metadata child must be inside its source prefix")
        quadrants.append((x >> (dz - 1), y >> (dz - 1)))
    if len(set(quadrants)) != len(quadrants):
        raise ValueError("overlapping metadata child source prefixes")
    present = [c.bound for c in children if c.bound.count]
    bounds = None
    if present:
        boxes = np.asarray([c.bounds for c in present])
        bounds = (tuple(boxes[:, 0].min(axis=0)), tuple(boxes[:, 1].max(axis=0)))
    digest = hashlib.sha256(
        json.dumps(
            [key, [dataclasses.asdict(c) for c in children]], sort_keys=True
        ).encode()
    ).hexdigest()
    bound = CourseSummary(
        key,
        sum(c.bound.count for c in children),
        bounds,
        min(c.minimum_level_m for c in present) if present else None,
        digest,
        all(c.bound.complete for c in children),
    )
    return MetadataRef(bound, sum(c.source_cells for c in children), False)


def metadata_pages(expected, summaries):
    expected = tuple(sorted(tile_tuple(key) for key in expected))
    if (
        not 1 <= len(expected) <= 65536
        or len(set(expected)) != len(expected)
        or len({k[0] for k in expected}) != 1
    ):
        raise ValueError("bounded declared source keys at one generation zoom required")
    refs = {}
    for key in expected:
        summary = summaries.get(key)
        if summary is None:
            summary = CourseSummary(key, 0, None, None, "unknown-metadata", False)
        if summary.key != key:
            raise ValueError("source metadata identity mismatch")
        refs[key] = MetadataRef(summary, 1, True)
    pages = {}

    def build(keys, prefix):
        if len(keys) == 1:
            return refs[keys[0]]
        level = prefix[0] + 1
        groups = {}
        for key in keys:
            shift = key[0] - level
            child = (level, key[1] >> shift, key[2] >> shift)
            groups.setdefault(child, []).append(key)
        children = tuple(build(items, key) for key, items in sorted(groups.items()))
        if len(children) == 1:
            return children[0]
        reference = aggregate(prefix, children)
        pages[prefix] = children
        return reference

    return build(expected, (0, 0, 0)), pages
