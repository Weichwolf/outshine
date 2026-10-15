"""Source-preserving coalescing of unambiguous ranked material regions."""

from collections import defaultdict
from dataclasses import dataclass

import shapely
from shapely.geometry.base import BaseGeometry

from ..contracts import LocalFrame, geometry, identifier


@dataclass(frozen=True, slots=True)
class MaterialRegion:
    frame: LocalFrame
    material_row: int
    rank: int
    geometry: BaseGeometry
    source_ids: tuple[str, ...]

    def __post_init__(self):
        if not isinstance(self.frame, LocalFrame):
            raise TypeError("material regions require one named local frame")
        if any(
            type(value) is not int or value < 0
            for value in (self.material_row, self.rank)
        ):
            raise ValueError("material row and rank require nonnegative integers")
        geometry(self.geometry, polygonal=True)
        if not isinstance(self.source_ids, tuple) or not self.source_ids:
            raise ValueError("material regions require immutable source identities")
        for value in self.source_ids:
            identifier(value)
        if len(set(self.source_ids)) != len(self.source_ids):
            raise ValueError("material source identities must be distinct")


def coalesce_regions(regions):
    """Union equal material/rank only where that rank names one material.

    Ambiguous ranks retain the original records and their order. Higher-rank
    priority and per-rank class coverage stay unchanged. Complete source records
    remain caller-owned; outputs retain all contributing IDs and the local frame.
    This is an offline geometric reference, not a frame-time or GPU guarantee.
    """
    if not isinstance(regions, tuple) or any(
        not isinstance(row, MaterialRegion) for row in regions
    ):
        raise ValueError("an immutable tuple of MaterialRegion is required")
    if regions and any(row.frame != regions[0].frame for row in regions):
        raise ValueError("material regions must share the same named local frame")
    rank_rows = defaultdict(set)
    for region in regions:
        rank_rows[region.rank].add(region.material_row)
    grouped = defaultdict(list)
    for index, region in enumerate(regions):
        key = (
            (region.rank, region.material_row)
            if len(rank_rows[region.rank]) == 1
            else (index,)
        )
        grouped[key].append(region)
    output = []
    for members in grouped.values():
        first = members[0]
        if len(members) == 1:
            output.append(first)
            continue
        merged = shapely.union_all([member.geometry for member in members])
        sources = tuple(
            sorted({source for member in members for source in member.source_ids})
        )
        output.append(
            MaterialRegion(first.frame, first.material_row, first.rank, merged, sources)
        )
    return tuple(output)
