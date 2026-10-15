"""Verified cached Terrarium samples in geodetic coordinates; missing DEM stays explicit."""

import hashlib
from pathlib import Path

import numpy as np
from PIL import Image


class Elevation:
    def __init__(self, receipts):
        self.receipts = {
            tuple(row["tile"]): row
            for row in receipts
            if row["kind"] == "elevation" and not row.get("missing")
        }
        self.rasters = {}

    def raster(self, tile):
        if tile not in self.rasters:
            record = self.receipts[tile]
            data = Path(record["path"]).read_bytes()
            if hashlib.sha256(data).hexdigest() != record["sha256"]:
                raise ValueError("Pinned Place DEM bytes changed")
            with Image.open(record["path"]) as image:
                pixels = np.asarray(image.convert("RGB"), dtype=np.float64)
            self.rasters[tile] = (
                pixels[..., 0] * 256 + pixels[..., 1] + pixels[..., 2] / 256 - 32768
            )
        return self.rasters[tile]

    def sample(self, latitude, longitude):
        latitude, longitude = np.broadcast_arrays(latitude, longitude)
        if (
            not np.isfinite(latitude).all()
            or not np.isfinite(longitude).all()
            or np.any(abs(latitude) >= 85.05112878)
            or np.any(abs(longitude) >= 180)
        ):
            raise ValueError(
                "Place DEM samples require finite Web-Mercator coordinates"
            )
        shape = latitude.shape
        latitude, longitude = latitude.ravel(), longitude.ravel()
        world = np.column_stack(
            [
                (longitude + 180) / 360,
                (1 - np.arcsinh(np.tan(np.deg2rad(latitude))) / np.pi) / 2,
            ]
        )
        result = np.full(len(latitude), np.nan)
        for zoom in sorted({tile[0] for tile in self.receipts}, reverse=True):
            unresolved = np.flatnonzero(~np.isfinite(result))
            coordinates = world[unresolved] * (1 << zoom)
            cells, inverse = np.unique(
                np.floor(coordinates).astype(int), axis=0, return_inverse=True
            )
            for index, cell in enumerate(cells):
                tile = zoom, int(cell[0]), int(cell[1])
                if tile not in self.receipts:
                    continue
                height = self.raster(tile)
                selected = np.flatnonzero(inverse == index)
                pixels = (coordinates[selected] - cell) * [
                    height.shape[1] - 1,
                    height.shape[0] - 1,
                ]
                base = np.minimum(
                    np.floor(pixels).astype(int),
                    [height.shape[1] - 2, height.shape[0] - 2],
                )
                u, v = (pixels - base).T
                x, y = base.T
                result[unresolved[selected]] = (1 - v) * (
                    (1 - u) * height[y, x] + u * height[y, x + 1]
                ) + v * ((1 - u) * height[y + 1, x] + u * height[y + 1, x + 1])
        if not np.isfinite(result).all():
            raise ValueError("Place DEM does not cover all requested samples")
        return result.reshape(shape)
