"""Sample existing native height pages for clipmap experiments; no acquisition."""

from dataclasses import dataclass
from pathlib import Path

import numpy as np

from terrain_cached_page import eligible, package, page_fractions, page_prefix, page_records
from terrain_geo import ecef


@dataclass(frozen=True)
class PageLevel:
    keys: np.ndarray
    heights: np.ndarray
    fractions: np.ndarray


class NativeTerrain:
    def __init__(self, anchor, pages, provenance=None):
        self.anchor = np.asarray(anchor, dtype=np.float64)
        if self.anchor.shape != (2,) or not np.all(np.isfinite(self.anchor)) or abs(self.anchor[1]) > 90:
            raise ValueError("a finite geographic anchor is required")
        self.provenance = {} if provenance is None else provenance
        groups = {}
        for row, nodes in pages:
            if not eligible(row):
                raise ValueError("the native height-page layout is not supported")
            height = np.frombuffer(nodes, dtype="<f4").reshape(35, 35)
            if not np.all(np.isfinite(height)):
                raise ValueError("native height pages must be finite")
            groups.setdefault(row["zoom"], []).append(
                ((row["x"] << 32) | row["y"], height, page_fractions(row))
            )
        self.pages = {}
        for zoom, entries in groups.items():
            entries.sort(key=lambda item: item[0])
            keys = np.array([entry[0] for entry in entries], dtype=np.uint64)
            if np.any(keys[1:] == keys[:-1]):
                raise ValueError("ambiguous duplicate native page")
            self.pages[zoom] = PageLevel(keys, np.stack([p[1] for p in entries]),
                                         np.stack([p[2] for p in entries]))
        if not self.pages:
            raise ValueError("native height pages are required")
        lon, lat = np.deg2rad(self.anchor)
        self.origin = ecef(*self.anchor)
        self.axes = np.array([
            [-np.sin(lon), np.cos(lon), 0],
            [-np.sin(lat) * np.cos(lon), -np.sin(lat) * np.sin(lon), np.cos(lat)],
            [np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)],
        ])

    @classmethod
    def from_cache(cls, cache, key, byte_budget):
        data, provenance = package(Path(cache), key, max_bytes=byte_budget)
        reader, anchor, count = page_prefix(data)
        result = cls(anchor, page_records(reader, count), provenance)
        result.provenance.update(anchor_lon_lat=list(anchor), pages=count,
                                 scope="native versioned height-page prefix; tail not revalidated")
        return result

    def heights_lon_lat(self, lon_lat):
        lon_lat = np.asarray(lon_lat, dtype=np.float64)
        if lon_lat.ndim != 2 or lon_lat.shape[1] != 2:
            raise ValueError("longitude/latitude pairs are required")
        result = np.full(len(lon_lat), np.nan)
        finite = np.all(np.isfinite(lon_lat), axis=1) & (np.abs(lon_lat[:, 1]) <= 85.0511287798066)
        x = (np.where(finite, lon_lat[:, 0], 0) + 180) / 360
        y = (1 - np.arcsinh(np.tan(np.deg2rad(np.where(finite, lon_lat[:, 1], 0)))) / np.pi) / 2
        for zoom in sorted(self.pages, reverse=True):
            pending = np.flatnonzero(finite & np.isnan(result))
            if not len(pending):
                break
            scale = 1 << zoom
            points = np.column_stack((np.mod(x[pending], 1), y[pending])) * scale
            tile = np.floor(points).astype(np.int64)
            keys = (tile[:, 0].astype(np.uint64) << np.uint64(32)) | tile[:, 1].astype(np.uint64)
            level = self.pages[zoom]
            index = np.searchsorted(level.keys, keys)
            safe = np.minimum(index, len(level.keys) - 1)
            found = (index < len(level.keys)) & (level.keys[safe] == keys)
            selected = safe[found]
            uv = points[found] - tile[found]
            result[pending[found]] = self.interpolate(level, selected, uv)
        return result

    @staticmethod
    def interpolate(level, pages, uv):
        cells = np.minimum((uv * 32).astype(np.int64), 31)
        fractions = level.fractions[pages]
        for axis in range(2):
            candidate = cells[:, axis]
            advance = uv[:, axis] >= fractions[np.arange(len(pages)), candidate + 1]
            cells[:, axis] = np.minimum(candidate + advance, 31)
        offsets = np.column_stack([
            fractions[np.arange(len(pages)), cells[:, axis]] for axis in range(2)
        ])
        widths = np.column_stack([
            fractions[np.arange(len(pages)), cells[:, axis] + 1] for axis in range(2)
        ]) - offsets
        u, v = ((uv - offsets) / widths).T
        i, j = cells.T + 1
        a = level.heights[pages, j, i]
        b = level.heights[pages, j, i + 1]
        c = level.heights[pages, j + 1, i + 1]
        d = level.heights[pages, j + 1, i]
        return np.where(u >= v, (1 - u) * a + (u - v) * b + v * c,
                        (1 - v) * a + (v - u) * d + u * c)

    def surface_lon_lat(self, xy):
        base = self.origin + xy @ self.axes[:2]
        inverse_radii2 = 1 / np.array([6378137.0, 6378137.0, 6356752.314245179]) ** 2
        up = self.axes[2]
        a = np.sum(up * up * inverse_radii2)
        b = (base * inverse_radii2) @ up
        c = np.sum(base * base * inverse_radii2, axis=1) - 1
        discriminant = b * b - a * c
        if np.any(discriminant < 0):
            raise ValueError("the local height-field projection does not cover this position")
        height = -c / (b + np.sqrt(discriminant))
        surface = base + height[:, None] * up
        lon = np.arctan2(surface[:, 1], surface[:, 0])
        lat = np.arctan2(surface[:, 2] / (1 - 0.0066943799901413165),
                         np.hypot(surface[:, 0], surface[:, 1]))
        return np.rad2deg(np.column_stack((lon, lat)))

    def __call__(self, xy):
        xy = np.asarray(xy, dtype=np.float64)
        if xy.ndim != 2 or xy.shape[1] != 2 or not np.all(np.isfinite(xy)):
            raise ValueError("finite local east/north positions required")
        query = xy.copy()
        heights = np.full(len(xy), np.nan)
        for _ in range(3):
            lon_lat = self.surface_lon_lat(query)
            sampled = self.heights_lon_lat(lon_lat)
            known = np.isfinite(sampled)
            heights.fill(np.nan)
            if not np.any(known):
                break
            point = ecef(lon_lat[known, 0], lon_lat[known, 1], sampled[known]) - self.origin
            query[known] += xy[known] - point @ self.axes[:2].T
            heights[known] = point @ self.axes[2]
        return heights
