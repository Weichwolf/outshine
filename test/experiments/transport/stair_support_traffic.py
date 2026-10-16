"""Prepared actual traffic triangles; source masks distinguish shared and foreign faces."""

from dataclasses import dataclass

import numpy as np
import shapely


class TrafficIndex:
    def __init__(self, shapes, source_masks):
        original = np.asarray(source_masks)
        if original.ndim != 1 or len(original) != len(shapes) or len(original) > 65536:
            raise ValueError(
                "bounded matching traffic shapes and source masks required"
            )
        if original.size and original.dtype.kind not in "iu":
            raise ValueError("integer traffic source masks required")
        masks = np.array(original, dtype=np.int64, copy=True)
        if np.any(masks < 0) or np.any((masks == 0) & ~shapely.is_empty(shapes)):
            raise ValueError("nonempty traffic requires positive source masks")
        masks.setflags(write=False)
        self.source_masks = masks
        self.tree = shapely.STRtree(shapes)

    def for_source(self, owner):
        if type(owner) is not int or not 0 <= owner < 63:
            raise ValueError("bounded traffic owner bit required")
        return TrafficMask(self, owner)


@dataclass(frozen=True, slots=True)
class TrafficMask:
    index: TrafficIndex
    owner: int

    def conflicts(self, shape):
        hits = self.index.tree.query(shape, predicate="dwithin", distance=0.2)
        return bool(np.any((self.index.source_masks[hits] & ~(1 << self.owner)) != 0))


@dataclass(frozen=True, slots=True)
class RegionMask:
    """Unchanged whole-union distance test retained for the cost reference."""

    region: object

    def conflicts(self, shape):
        return bool(shapely.dwithin(shape, self.region, 0.2))
