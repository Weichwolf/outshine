"""Best-first metadata page queries; unavailable data never selects a fallback.

The caller supplies a fixed authoritative root and an immutable page loader.
No source acquisition, database, page cache or native format is implemented.
"""

import dataclasses
import hashlib
import heapq
import json

import numpy as np
from water_course_envelope import block_lower_bound, block_minimum, summarize
from water_envelope_pages import aggregate
from water_envelope_query import EnvelopeQuery


class HierarchicalEnvelope:
    def __init__(self, root, load_metadata, *, scope, slope=0.005):
        if (
            not isinstance(scope, str)
            or not scope
            or not callable(load_metadata)
            or not np.isfinite(slope)
            or not 1e-6 <= slope <= 1
        ):
            raise ValueError(
                "explicit immutable metadata root, loader and model scope required"
            )
        self.root, self.load_metadata, self.slope = root, load_metadata, float(slope)
        self.version = hashlib.sha256(
            json.dumps(
                [
                    "hierarchical-course-envelope-v1",
                    scope,
                    self.slope,
                    None if root is None else dataclasses.asdict(root),
                ],
                sort_keys=True,
            ).encode()
        ).hexdigest()

    def query(self, query, load, *, payload_limit=64, metadata_limit=64):
        query = np.asarray(query, dtype=float)
        if (
            query.shape != (3,)
            or not np.isfinite(query).all()
            or np.abs(query).max() > 1e8
        ):
            raise ValueError("finite bounded ECEF query required")
        if any(
            type(v) is not int or not 1 <= v <= 4096
            for v in (payload_limit, metadata_limit)
        ):
            raise ValueError("separate bounded metadata and payload budgets required")
        opened, pages, excluded = [], [], []
        work = {
            "tested_segments": 0,
            "bound_tests": 0,
            "metadata_entries": 0 if self.root is None else 1,
            "frontier_peak": 0,
            "excluded_source_cells": 0,
            "empty_source_cells": 0,
        }
        best = None

        def result(state, reason=None):
            details = dict(work, metadata_pages=tuple(pages), failure=reason)
            if state == "ready":
                return EnvelopeQuery(
                    state,
                    best[0],
                    best[1],
                    best[2],
                    tuple(opened),
                    tuple(excluded),
                    details,
                )
            return EnvelopeQuery(
                state, None, None, None, tuple(opened), tuple(excluded), details
            )

        if self.root is None or not self.root.bound.complete:
            return result(
                "pending_metadata", None if self.root is None else self.root.bound.key
            )
        work["bound_tests"] = work["frontier_peak"] = 1
        frontier = [
            (
                block_lower_bound(self.root.bound, query, self.slope),
                self.root.bound.key,
                self.root,
            )
        ]
        while frontier:
            lower, key, reference = heapq.heappop(frontier)
            if best is not None and lower > best[0]:
                for bound, address, ref in [(lower, key, reference), *frontier]:
                    excluded.append((address, float(bound)))
                    work["excluded_source_cells"] += ref.source_cells
                return result("ready")
            if not reference.bound.count:
                work["empty_source_cells"] += reference.source_cells
                continue
            if reference.leaf:
                if len(opened) == payload_limit:
                    return result("budget_exhausted", key)
                block = load(key)
                opened.append(key)
                if block is None:
                    return result("pending_payload", key)
                if summarize(block) != reference.bound:
                    return result("stale_payload", key)
                height, identity, parameter = block_minimum(block, query, self.slope)
                work["tested_segments"] += len(block.segments)
                candidate = (height, (key, identity), parameter)
                best = candidate if best is None else min(best, candidate)
            else:
                if len(pages) == metadata_limit:
                    return result("metadata_budget_exhausted", key)
                page = self.load_metadata(key)
                pages.append(key)
                if page is None:
                    return result("pending_metadata", key)
                try:
                    actual = aggregate(key, page)
                except ValueError:
                    return result("stale_metadata", key)
                if actual != reference:
                    return result("stale_metadata", key)
                work["metadata_entries"] += len(page)
                for child in page:
                    if child.bound.complete and not child.bound.count:
                        work["empty_source_cells"] += child.source_cells
                        continue
                    work["bound_tests"] += 1
                    bound = block_lower_bound(child.bound, query, self.slope)
                    heapq.heappush(frontier, (bound, child.bound.key, child))
                work["frontier_peak"] = max(work["frontier_peak"], len(frontier))
        return result("empty_guide" if best is None else "ready")
