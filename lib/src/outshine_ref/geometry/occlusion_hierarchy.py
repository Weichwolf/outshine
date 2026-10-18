"""Bounded source-envelope queries before geometry emission; cache cells are not inputs."""

from dataclasses import dataclass

import numpy as np

from .occlusion import OpaqueUmbra, _bounded_boxes, classify_boxes


@dataclass(frozen=True, slots=True)
class OcclusionSelection:
    hidden: np.ndarray
    observer_box: np.ndarray | None
    work: dict

    def contains_observer(self, eye):
        if self.observer_box is None:
            return True
        eye = np.asarray(eye, dtype=np.float64)
        return (
            self.observer_box is not None
            and eye.shape == (3,)
            and np.isfinite(eye).all()
            and bool(
                ((eye >= self.observer_box[0]) & (eye <= self.observer_box[1])).all()
            )
        )


class BoundsHierarchy:
    """Immutable envelope hierarchy. Unknown bounds always remain selected.

    Source indices are the caller's immutable record indices. An occluder pair is
    (OpaqueUmbra, owner_source_index_or_None). Opacity is never inferred here.
    """

    def __init__(self, bounds, *, known, leaf_size=8):
        held = np.array(bounds, dtype=np.float64, copy=True)
        flags = np.asarray(known)
        if (
            held.ndim != 3
            or held.shape[1:] != (2, 3)
            or len(held) > 262144
            or flags.dtype.kind != "b"
            or flags.shape != (len(held),)
            or type(leaf_size) is not int
            or not 1 <= leaf_size <= 64
        ):
            raise ValueError(
                "bounded source boxes, explicit known flags and leaf size required"
            )
        _bounded_boxes(held[flags])
        self.bounds = held
        self.order = np.flatnonzero(flags)
        self.leaf_size = leaf_size
        rows = []
        if len(self.order):
            self._build(rows, 0, len(self.order))
        dtype = [
            ("bounds", "<f8", (2, 3)),
            ("first", "<i8"),
            ("count", "<i8"),
            ("left", "<i8"),
            ("right", "<i8"),
        ]
        self.nodes = np.array(rows, dtype=dtype)
        self.position = np.full(len(held), -1, dtype=np.int64)
        self.position[self.order] = np.arange(len(self.order))
        for array in (self.bounds, self.order, self.nodes, self.position):
            array.setflags(write=False)

    def _build(self, rows, first, count):
        indices = self.order[first : first + count]
        boxes = self.bounds[indices]
        enclosure = np.array([boxes[:, 0].min(axis=0), boxes[:, 1].max(axis=0)])
        node = len(rows)
        rows.append((enclosure, first, count, -1, -1))
        if count > self.leaf_size:
            centres = (boxes[:, 0] + boxes[:, 1]) * 0.5
            axis = np.ptp(centres, axis=0).argmax()
            half = count // 2
            self.order[first : first + count] = indices[
                np.argpartition(centres[:, axis], half)
            ]
            left = self._build(rows, first, half)
            right = self._build(rows, first + half, count - half)
            rows[node] = (enclosure, first, count, left, right)
        return node

    def _leaf(self, node, umbra, owner, protected, hidden, work, limit, margin):
        first, count = int(node["first"]), int(node["count"])
        ids = self.order[first : first + count]
        work["source_flag_reads"] += len(ids) * 2
        ids = ids[~hidden[ids]]
        if protected is not None:
            work["protected_flag_reads"] += len(ids)
            ids = ids[~protected[ids]]
        if owner is not None:
            ids = ids[ids != owner]
        ids = ids[: limit - work["box_tests"]]
        inside, _ = classify_boxes(self.bounds[ids], umbra, margin_m=margin)
        work["box_tests"] += len(ids)
        work["source_box_tests"] += len(ids)
        work["plane_tests"] += len(ids) * len(umbra.low)
        hidden[ids[inside]] = True
        work["source_flag_writes"] += int(inside.sum())
        return bool(hidden[self.order[first : first + count]].all())

    def _query(
        self, umbra, owner, protected, prefix, hidden, done, work, limit, margin
    ):
        stack = [0] if len(self.nodes) else []
        owner_position = -1 if owner is None else int(self.position[owner])
        while stack and work["box_tests"] < limit:
            pending = [
                stack.pop()
                for _ in range(min(512, len(stack), limit - work["box_tests"]))
            ]
            work["node_flag_reads"] += len(pending)
            pending = np.asarray([n for n in pending if not done[n]], dtype=int)
            if not len(pending):
                continue
            rows = self.nodes[pending]
            inside, outside = classify_boxes(rows["bounds"], umbra, margin_m=margin)
            work["box_tests"] += len(pending)
            work["nodes_visited"] += len(pending)
            work["plane_tests"] += len(pending) * len(umbra.low)
            for index, row in enumerate(rows):
                if outside[index]:
                    continue
                first, count = int(row["first"]), int(row["count"])
                blocked = prefix is not None and prefix[first + count] != prefix[first]
                owns = first <= owner_position < first + count
                if inside[index] and not blocked and not owns:
                    hidden[self.order[first : first + count]] = True
                    done[pending[index]] = True
                    work["source_flag_writes"] += count
                    work["accepted_nodes"] += 1
                elif row["left"] < 0:
                    done[pending[index]] = self._leaf(
                        row, umbra, owner, protected, hidden, work, limit, margin
                    )
                else:
                    stack.extend((int(row["left"]), int(row["right"])))

    def select(
        self, occluders, *, protected=None, max_box_tests=1_000_000, margin_m=1e-6
    ):
        """Return conservative hidden flags; exhausted work retains unproved sources."""
        if (
            type(max_box_tests) is not int
            or not 0 <= max_box_tests <= 1_000_000_000
            or not np.isfinite(margin_m)
            or margin_m < 0
        ):
            raise ValueError(
                "explicit nonnegative bounded work and world margin required"
            )
        if len(occluders) > 4096:
            raise ValueError("bounded occluder packet required")
        prefix = None
        if protected is not None:
            protected = np.asarray(protected)
            if protected.dtype.kind != "b" or protected.shape != (len(self.bounds),):
                raise ValueError("explicit per-source protection flags required")
            prefix = np.r_[0, np.cumsum(protected[self.order], dtype=np.int64)]
        hidden, done = (
            np.zeros(len(self.bounds), dtype=bool),
            np.zeros(len(self.nodes), dtype=bool),
        )
        work = {
            "box_tests": 0,
            "source_box_tests": 0,
            "nodes_visited": 0,
            "plane_tests": 0,
            "source_flag_reads": 0,
            "source_flag_writes": 0,
            "accepted_nodes": 0,
            "node_flag_reads": 0,
            "protected_flag_reads": len(self.order) if protected is not None else 0,
        }
        region = None
        for umbra, owner in occluders:
            if not isinstance(umbra, OpaqueUmbra) or (
                owner is not None
                and (type(owner) is not int or not 0 <= owner < len(hidden))
            ):
                raise ValueError(
                    "explicit umbra and valid optional source owner required"
                )
            if not len(umbra.low):
                continue
            if (
                owner is not None
                and self.position[owner] >= 0
                and (
                    (umbra.patch_bounds[0] < self.bounds[owner, 0]).any()
                    or (umbra.patch_bounds[1] > self.bounds[owner, 1]).any()
                )
            ):
                raise ValueError("opaque patch escapes its declared source envelope")
            if region is not None and not np.array_equal(region, umbra.observer_box):
                raise ValueError("one common observer region per selection required")
            region = umbra.observer_box
            self._query(
                umbra,
                owner,
                protected,
                prefix,
                hidden,
                done,
                work,
                max_box_tests,
                margin_m,
            )
        work["budget_exhausted"] = work["box_tests"] >= max_box_tests
        if not work["source_flag_writes"]:
            region = None
        hidden.setflags(write=False)
        return OcclusionSelection(hidden, region, work)
