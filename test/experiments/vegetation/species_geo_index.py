"""Compare compact GeoCellId candidate lookup with exact botanical-region polygons."""

import json
from pathlib import Path

import numpy as np
import shapely
from shapely.affinity import translate

MIXED = 1 << 15


def read_regions(path, *, union_multipart=False, wrap_longitude=False):
    path = Path(path)
    if not 0 < path.stat().st_size <= 4 * 1024**2:
        raise ValueError("bounded explicit botanical-region GeoJSON required")
    collection = json.loads(path.read_bytes())
    features = collection["features"]
    if collection["type"] != "FeatureCollection" or not 0 < len(features) < MIXED:
        raise ValueError("bounded region feature collection required")
    codes, geometries, normalizations, seen = [], [], [], set()
    for feature in features:
        code = feature["properties"]["LEVEL3_COD"]
        geometry = shapely.from_geojson(json.dumps(feature["geometry"]))
        if (
            union_multipart
            and geometry.geom_type == "MultiPolygon"
            and not geometry.is_valid
        ):
            parts = shapely.get_parts(geometry)
            if shapely.is_valid(parts).all():
                original_area, reason = geometry.area, shapely.is_valid_reason(geometry)
                geometry = shapely.union_all(parts)
                normalizations.append(
                    {
                        "code": code,
                        "reason": reason,
                        "operation": "set union of valid sibling polygons; original source retained",
                        "summed_source_area_deg2": original_area,
                        "union_area_deg2": geometry.area,
                    }
                )
        if (
            not isinstance(code, str)
            or not code
            or code in seen
            or geometry.geom_type not in ("Polygon", "MultiPolygon")
            or geometry.is_empty
            or not geometry.is_valid
        ):
            raise ValueError(
                "unique region code and valid nonempty source polygon required"
            )
        coordinates = shapely.get_coordinates(geometry)
        if (
            not np.isfinite(coordinates).all()
            or np.any(abs(coordinates[:, 1]) > 90)
            or np.any(abs(coordinates[:, 0]) > 540)
        ):
            raise ValueError("finite WGS84 source polygon required")
        if np.any(abs(coordinates[:, 0]) > 180):
            if not wrap_longitude:
                raise ValueError(
                    "source longitude needs explicit periodic normalization"
                )
            original_area = geometry.area
            world = shapely.box(-180, -90, 180, 90)
            geometry = shapely.union_all(
                [
                    shapely.intersection(translate(geometry, xoff=shift), world)
                    for shift in (-360, 0, 360)
                ]
            )
            normalizations.append(
                {
                    "code": code,
                    "operation": "periodic longitude split at world meridian; original source retained",
                    "source_minimum_longitude": float(coordinates[:, 0].min()),
                    "source_maximum_longitude": float(coordinates[:, 0].max()),
                    "source_area_deg2": original_area,
                    "canonical_area_deg2": geometry.area,
                }
            )
        codes.append(code)
        seen.add(code)
        geometries.append(geometry)
    order = np.argsort(codes)
    return (
        tuple(codes[i] for i in order),
        np.asarray(geometries, dtype=object)[order],
        normalizations,
    )


def points(latitude, longitude, *, max_queries=1000000):
    latitude, longitude = np.broadcast_arrays(latitude, longitude)
    if (
        latitude.size > max_queries
        or not np.isfinite(latitude).all()
        or not np.isfinite(longitude).all()
        or np.any(abs(latitude) > 90)
        or np.any(abs(longitude) > 180)
    ):
        raise ValueError("bounded finite WGS84 query coordinates required")
    return np.column_stack(
        (np.where(longitude.ravel() == 180, -180, longitude.ravel()), latitude.ravel())
    )


def ordered(rows, regions):
    rows, regions = (
        np.asarray(rows, dtype=np.int64),
        np.asarray(regions, dtype=np.int64),
    )
    order = np.lexsort((regions, rows))
    return np.array((rows[order], regions[order]))


class DirectRegions:
    def __init__(self, geometries):
        self.geometries = np.asarray(geometries, dtype=object)
        if not 0 < len(self.geometries) < MIXED:
            raise ValueError("bounded nonempty region source required")
        self.tree = shapely.STRtree(self.geometries)

    def query(self, latitude, longitude):
        xy = points(latitude, longitude)
        pairs = self.tree.query(shapely.points(xy), predicate="covered_by")
        return ordered(*pairs)


class CellRegions:
    def __init__(self, direct, *, level=8):
        if type(level) is not int or not 0 <= level <= 10:
            raise ValueError("bounded GeoCellId level required")
        self.source, self.level, self.side = direct.geometries, level, 1 << level
        self.table = np.zeros(self.side**2, dtype=np.uint16)
        buckets, interned = [], {}
        batch = min(1024, 1048576 // len(self.source))
        for begin in range(0, len(self.table), batch):
            keys = np.arange(begin, min(begin + batch, len(self.table)))
            x, y = keys % self.side, keys // self.side
            west, south = -180 + x * 360 / self.side, -90 + y * 180 / self.side
            cells = shapely.box(
                west, south, west + 360 / self.side, south + 180 / self.side
            )
            pairs = direct.tree.query(cells, predicate="intersects")
            if not pairs.shape[1]:
                continue
            order = np.lexsort((pairs[1], pairs[0]))
            rows, owners = pairs[:, order]
            counts = np.bincount(rows, minlength=len(cells))
            unique = counts[rows] == 1
            interior = np.zeros(len(rows), bool)
            interior[unique] = shapely.covers(
                self.source[owners[unique]], cells[rows[unique]]
            )
            self.table[keys[rows[interior]]] = owners[interior] + 1
            boundary = ~interior
            selected_rows, selected_owners = rows[boundary], owners[boundary]
            edges = np.r_[
                0, np.flatnonzero(np.diff(selected_rows)) + 1, len(selected_rows)
            ]
            for first, last in zip(edges[:-1], edges[1:], strict=True):
                if first == last:
                    continue
                candidates = tuple(map(int, selected_owners[first:last]))
                if candidates not in interned:
                    if len(buckets) >= MIXED:
                        raise ValueError("mixed-region table exceeds uint16 layout")
                    interned[candidates] = len(buckets)
                    buckets.append(candidates)
                self.table[keys[selected_rows[first]]] = MIXED + interned[candidates]
        lengths = np.array([len(b) for b in buckets], dtype=np.uint32)
        self.offsets = np.r_[np.uint32(0), np.cumsum(lengths, dtype=np.uint32)]
        self.candidates = np.array([i for b in buckets for i in b], dtype=np.uint16)

    def query(self, latitude, longitude, *, max_candidates=4000000):
        if type(max_candidates) is not int or not 0 <= max_candidates <= 16000000:
            raise ValueError("explicit candidate budget required")
        xy = points(latitude, longitude)
        # Exact ordered edge comparisons avoid losing sub-ULP offsets when
        # adding 180/90 before floor at an internal cell boundary.
        cell = np.column_stack(
            (
                np.searchsorted(
                    np.linspace(-180, 180, self.side + 1), xy[:, 0], side="right"
                )
                - 1,
                np.searchsorted(
                    np.linspace(-90, 90, self.side + 1), xy[:, 1], side="right"
                )
                - 1,
            )
        )
        cell[:, 1] = np.minimum(cell[:, 1], self.side - 1)
        values = self.table[cell[:, 1] * self.side + cell[:, 0]]
        fast = np.flatnonzero((values > 0) & (values < MIXED))
        all_rows, all_regions = [fast], [values[fast].astype(np.int64) - 1]
        mixed = np.flatnonzero(values >= MIXED)
        buckets = values[mixed].astype(np.int64) - MIXED
        if (
            int(
                np.sum(
                    self.offsets[buckets + 1] - self.offsets[buckets], dtype=np.int64
                )
            )
            > max_candidates
        ):
            raise ValueError("region candidate budget exceeded before pair allocation")
        for bucket in np.unique(buckets):
            selected = mixed[buckets == bucket]
            owners = self.candidates[self.offsets[bucket] : self.offsets[bucket + 1]]
            rows = np.repeat(selected, len(owners))
            regions = np.tile(owners, len(selected))
            keep = shapely.covers(self.source[regions], shapely.points(xy[rows]))
            all_rows.append(rows[keep])
            all_regions.append(regions[keep])
        return ordered(np.concatenate(all_rows), np.concatenate(all_regions))

    def summary(self):
        return {
            "level": self.level,
            "cells": len(self.table),
            "empty_cells": int(np.count_nonzero(self.table == 0)),
            "interior_cells": int(
                np.count_nonzero((self.table > 0) & (self.table < MIXED))
            ),
            "boundary_cells": int(np.count_nonzero(self.table >= MIXED)),
            "shared_candidate_lists": len(self.offsets) - 1,
            "table_bytes": self.table.nbytes,
            "candidate_bytes": self.offsets.nbytes + self.candidates.nbytes,
            "limits": "Exact source polygon predicates at boundaries, deterministic source membership pairs; world country regions are not habitats or species coverage. Source geometry storage and Python/GEOS object cost excluded from array sizes.",
        }
