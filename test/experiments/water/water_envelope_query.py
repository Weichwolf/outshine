"""Metadata before payload queries of one declared finite source-layer snapshot.

The expected key set is supplied independently of resident payloads. It is not a
claim that these sources describe every watercourse on Earth. No DB format,
hierarchy, source acquisition or native asset ABI is defined here.
"""

import dataclasses
import hashlib
import json
from dataclasses import dataclass

import numpy as np
from water_course_envelope import block_lower_bound, block_minimum, summarize


@dataclass(frozen=True)
class EnvelopeQuery:
    state: str
    height_m: float | None
    winner: tuple | None
    parameter: float | None
    opened: tuple
    excluded: tuple
    reason: object = None


class EnvelopeIndex:
    def __init__(self, expected, summaries, *, scope, slope=0.005):
        self.expected = tuple(sorted(expected))
        if not 0 < len(self.expected) <= 1024 or len(set(self.expected)) != len(
            self.expected
        ):
            raise ValueError("explicit bounded finite expected source keys required")
        if (
            not isinstance(scope, str)
            or not scope
            or not np.isfinite(slope)
            or not 1e-6 <= slope <= 1
        ):
            raise ValueError("explicit snapshot scope and finite model slope required")
        self.slope = float(slope)
        self.summaries = {key: summaries.get(key) for key in self.expected}
        for key, summary in self.summaries.items():
            if summary is None:
                continue
            if not isinstance(summary.digest, str) or not summary.digest:
                raise ValueError("versioned source summary required")
            if (
                summary.key != key
                or type(summary.count) is not int
                or not 0 <= summary.count <= 8192
                or type(summary.complete) is not bool
            ):
                raise ValueError("source summary identity/count/completeness mismatch")
            if summary.count and (
                np.asarray(summary.bounds).shape != (2, 3)
                or not np.isfinite(summary.bounds).all()
                or np.any(np.asarray(summary.bounds)[0] > np.asarray(summary.bounds)[1])
                or not np.isfinite(summary.minimum_level_m)
            ):
                raise ValueError(
                    "finite ordered source bounds and minimum height required"
                )
            if not summary.count and (
                summary.bounds is not None or summary.minimum_level_m is not None
            ):
                raise ValueError("explicit empty source summary required")
        self.version = hashlib.sha256(
            json.dumps(
                [
                    "source-course-envelope-v1",
                    scope,
                    self.slope,
                    [
                        (key, None if s is None else dataclasses.asdict(s))
                        for key, s in self.summaries.items()
                    ],
                ],
                sort_keys=True,
            ).encode()
        ).hexdigest()

    def query(self, query, load, *, payload_limit=64):
        query = np.asarray(query, dtype=float)
        if (
            query.shape != (3,)
            or not np.isfinite(query).all()
            or np.abs(query).max() > 1e8
        ):
            raise ValueError("finite bounded ECEF reference point required")
        if type(payload_limit) is not int or not 1 <= payload_limit <= 1024:
            raise ValueError("bounded payload request budget required")
        missing = tuple(
            k for k, s in self.summaries.items() if s is None or not s.complete
        )
        if missing:
            return EnvelopeQuery("pending_metadata", None, None, None, (), (), missing)
        ordered = sorted(
            (block_lower_bound(s, query, self.slope), k)
            for k, s in self.summaries.items()
            if s.count
        )
        best, opened, tested = None, [], 0
        for position, (lower, key) in enumerate(ordered):
            if best is not None and lower > best[0]:
                excluded = tuple((k, float(bound)) for bound, k in ordered[position:])
                return EnvelopeQuery(
                    "ready",
                    best[0],
                    best[1],
                    best[2],
                    tuple(opened),
                    excluded,
                    {"tested_segments": tested},
                )
            if len(opened) == payload_limit:
                return EnvelopeQuery(
                    "budget_exhausted", None, None, None, tuple(opened), (), key
                )
            block = load(key)
            opened.append(key)
            if block is None:
                return EnvelopeQuery(
                    "pending_payload", None, None, None, tuple(opened), (), key
                )
            if summarize(block) != self.summaries[key]:
                return EnvelopeQuery(
                    "stale_payload", None, None, None, tuple(opened), (), key
                )
            height, identity, parameter = block_minimum(block, query, self.slope)
            tested += len(block.segments)
            candidate = (height, (key, identity), parameter)
            best = candidate if best is None else min(best, candidate)
        if best is None:
            return EnvelopeQuery("empty_guide", None, None, None, tuple(opened), ())
        return EnvelopeQuery(
            "ready",
            best[0],
            best[1],
            best[2],
            tuple(opened),
            (),
            {"tested_segments": tested},
        )
