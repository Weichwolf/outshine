"""Source-complete geometric far-product candidates, independent of cache cells.

An accepted box is a point-set error candidate, never an opaque occluder or
an approved native product. Appearance, topology and product residency require
separate contracts. Unknown or protected sources always retain original plans.
"""

from dataclasses import dataclass

import numpy as np
from facing_intervals import length, point, sub
from outshine_ref.geometry.occlusion_hierarchy import BoundsHierarchy


def projection_diameter(box, observer, *, focal_px, width, height, near_m=0.1):
    """Upper pixel distance between any two box points, if either is in view.

    Holds for every observer position in the box and every orthonormal view
    orientation with this symmetric viewport. This is not an opacity bound.
    """
    box, observer = np.asarray(box, float), np.asarray(observer, float)
    if box.shape != (2, 3) or observer.shape != (2, 3):
        raise ValueError("two metric boxes required")
    if (
        not np.isfinite(box).all()
        or not np.isfinite(observer).all()
        or (box[0] > box[1]).any()
        or (observer[0] > observer[1]).any()
        or not np.isfinite(focal_px)
        or focal_px <= 0
        or type(width) is not int
        or type(height) is not int
        or not 1 <= width <= 16384
        or not 1 <= height <= 16384
        or not np.isfinite(near_m)
        or near_m <= 0
    ):
        raise ValueError(
            "finite ordered boxes and explicit perspective profile required"
        )
    span = sub(point(box[1]), point(box[0]))
    diameter = length(span)[1]
    first, second = (
        sub(point(box[0]), point(observer[1])),
        sub(point(observer[0]), point(box[1])),
    )
    gap = (
        np.maximum(np.maximum(first[0], second[0]), 0),
        np.maximum(np.maximum(first[1], second[1]), 0),
    )
    distance = length(gap)[0]
    ratios = np.array(
        [
            1.0,
            np.nextafter(width / (2 * focal_px), np.inf),
            np.nextafter(height / (2 * focal_px), np.inf),
        ]
    )
    factor = length(point(ratios))[1]
    depth = np.nextafter(distance / factor, -np.inf)
    denominator = np.nextafter(depth - diameter, -np.inf)
    if denominator <= near_m or not np.isfinite(denominator):
        return np.inf
    numerator = np.nextafter(np.nextafter(focal_px * factor, np.inf) * diameter, np.inf)
    return float(np.nextafter(numerator / denominator, np.inf))


@dataclass(frozen=True)
class SourceCut:
    # A node marked False delegates its complete member range to original plans.
    nodes: tuple[tuple[int, bool], ...]
    unknown: np.ndarray
    work: dict
    observer_box: np.ndarray
    profile: tuple[float, int, int, float]

    def contains_view(self, eye, *, focal_px, width, height, error_px):
        eye = np.asarray(eye, float)
        return (
            self.profile == (focal_px, width, height, error_px)
            and eye.shape == (3,)
            and np.isfinite(eye).all()
            and bool(
                ((eye >= self.observer_box[0]) & (eye <= self.observer_box[1])).all()
            )
        )


class FarProductHierarchy:
    def __init__(self, bounds, *, known, proxy_eligible):
        self.tree = BoundsHierarchy(bounds, known=known, leaf_size=1)
        eligible = np.asarray(proxy_eligible)
        if eligible.dtype.kind != "b" or eligible.shape != (len(bounds),):
            raise ValueError("explicit eligibility per original source required")
        order = self.tree.order
        self.ineligible = np.concatenate(
            ([0], np.cumsum(~eligible[order], dtype=np.int64))
        )
        self.unknown = np.flatnonzero(~np.asarray(known))
        self.ineligible.setflags(write=False)
        self.unknown.setflags(write=False)

    def members(self, node):
        row = self.tree.nodes[node]
        return self.tree.order[int(row["first"]) : int(row["first"] + row["count"])]

    def select(
        self,
        observer,
        *,
        focal_px,
        width,
        height,
        error_px,
        node_budget,
        protected=None,
    ):
        if (
            not np.isfinite(error_px)
            or error_px < 0
            or type(node_budget) is not int
            or node_budget < 0
        ):
            raise ValueError("finite nonnegative error and node-work budget required")
        # Validate the query even for an empty hierarchy or exhausted budget.
        projection_diameter(
            np.zeros((2, 3)), observer, focal_px=focal_px, width=width, height=height
        )
        prefix = None
        if protected is not None:
            flags = np.asarray(protected)
            if flags.dtype.kind != "b" or flags.shape != (len(self.tree.bounds),):
                raise ValueError(
                    "explicit protected flags per original source required"
                )
            prefix = np.concatenate(
                ([0], np.cumsum(flags[self.tree.order], dtype=np.int64))
            )
        pending = [0] if len(self.tree.nodes) else []
        rows = []
        work = {
            "node_tests": 0,
            "error_tests": 0,
            "proxy_sources": 0,
            "original_sources": len(self.unknown),
            "protected_flag_reads": len(self.tree.order) if prefix is not None else 0,
        }
        while pending:
            index = pending.pop()
            node = self.tree.nodes[index]
            first, count = int(node["first"]), int(node["count"])
            end = first + count
            if work["node_tests"] >= node_budget or error_px == 0:
                rows.append((index, False))
                work["original_sources"] += count
                continue
            work["node_tests"] += 1
            admissible = self.ineligible[end] == self.ineligible[first] and (
                prefix is None or prefix[end] == prefix[first]
            )
            accepted = False
            if count > 1 and admissible:
                work["error_tests"] += 1
                accepted = (
                    projection_diameter(
                        node["bounds"],
                        observer,
                        focal_px=focal_px,
                        width=width,
                        height=height,
                    )
                    <= error_px
                )
            if accepted:
                rows.append((index, True))
                work["proxy_sources"] += count
            elif count == 1:
                rows.append((index, False))
                work["original_sources"] += count
            else:
                pending.extend((int(node["right"]), int(node["left"])))
        work.update(
            cut_descriptors=len(rows),
            proxy_nodes=sum(proxy for _, proxy in rows),
            descriptor_payload_bytes=8 * len(rows),
            unknown_address_bytes=self.unknown.nbytes,
        )
        frozen_observer = np.frombuffer(
            np.asarray(observer, dtype="<f8").tobytes(), dtype="<f8"
        ).reshape(2, 3)
        work["observer_snapshot_bytes"] = frozen_observer.nbytes
        return SourceCut(
            tuple(rows),
            self.unknown,
            work,
            frozen_observer,
            (focal_px, width, height, error_px),
        )


def box_triangles(box):
    """Diagnostic coarse geometry only; its membership lives in the source cut."""
    box = np.asarray(box, float)
    vertices = np.array([[box[(i >> a) & 1, a] for a in range(3)] for i in range(8)])
    faces = np.array(
        [
            [0, 2, 3],
            [0, 3, 1],
            [4, 5, 7],
            [4, 7, 6],
            [0, 1, 5],
            [0, 5, 4],
            [2, 6, 7],
            [2, 7, 3],
            [0, 4, 6],
            [0, 6, 2],
            [1, 3, 7],
            [1, 7, 5],
        ]
    )
    return vertices[faces]
