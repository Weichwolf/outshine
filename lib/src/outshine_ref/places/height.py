"""Verified lossless Terrarium rasters and highest-declared-resolution sampling."""

import io

import numpy as np
from PIL import Image

from .sources import receipt_bytes, tile_tuple


def decode_terrarium(data):
    if not 12 <= len(data) <= 16 * 1024**2:
        raise ValueError("bounded complete Terrarium WebP payload required")
    data = bytes(data)
    if (
        data[:4] != b"RIFF"
        or data[8:12] != b"WEBP"
        or int.from_bytes(data[4:8], "little") + 8 != len(data)
    ):
        raise ValueError("one complete RIFF WebP container required")
    position, chunks = 12, []
    while position + 8 <= len(data):
        size = int.from_bytes(data[position + 4 : position + 8], "little")
        chunks.append(data[position : position + 4])
        position += 8 + size + (size & 1)
    if (
        position != len(data)
        or chunks.count(b"VP8L") != 1
        or any(c in chunks for c in (b"VP8 ", b"ALPH", b"ANIM", b"ANMF"))
    ):
        raise ValueError("opaque nonanimated lossless WebP required")
    with Image.open(io.BytesIO(data)) as image:
        if (
            image.format != "WEBP"
            or image.mode != "RGB"
            or image.n_frames != 1
            or not all(2 <= n <= 1024 for n in image.size)
        ):
            raise ValueError("bounded opaque RGB Terrarium raster required")
        pixels = np.asarray(image, dtype=np.float64)
    height = pixels[..., 0] * 256 + pixels[..., 1] + pixels[..., 2] / 256 - 32768
    if np.any((height < -11500) | (height > 9500)):
        raise ValueError("Terrarium height lies outside the declared Earth domain")
    return np.frombuffer(height.tobytes(), dtype=np.float64).reshape(height.shape)


class Elevation:
    def __init__(self, receipts):
        if not isinstance(receipts, (list, tuple)) or len(receipts) > 65536:
            raise ValueError("bounded explicit source receipts required")
        self.receipts, self._rasters = {}, {}
        for row in receipts:
            if row["kind"] != "elevation" or row.get("missing"):
                continue
            tile = tile_tuple(row["tile"])
            if tile in self.receipts and self.receipts[tile]["sha256"] != row["sha256"]:
                raise ValueError(
                    "conflicting source snapshots at the same elevation tile"
                )
            self.receipts[tile] = dict(row)

    def raster(self, tile):
        tile = tile_tuple(tile)
        if tile not in self._rasters:
            if tile not in self.receipts:
                raise ValueError("requested DEM tile is not declared available")
            self._rasters[tile] = decode_terrarium(receipt_bytes(self.receipts[tile]))
        return self._rasters[tile]

    def sample_with_zoom(self, latitude, longitude):
        latitude, longitude = np.broadcast_arrays(latitude, longitude)
        if (
            not np.isfinite(latitude).all()
            or not np.isfinite(longitude).all()
            or np.any(abs(latitude) >= 85.05112878)
            or np.any(abs(longitude) >= 180)
        ):
            raise ValueError(
                "finite Web-Mercator coordinates required for DEM sampling"
            )
        shape = latitude.shape
        latitude, longitude = latitude.ravel(), longitude.ravel()
        world = np.column_stack(
            (
                (longitude + 180) / 360,
                (1 - np.arcsinh(np.tan(np.deg2rad(latitude))) / np.pi) / 2,
            )
        )
        result = np.full(len(latitude), np.nan)
        used_zoom = np.full(len(latitude), -1, dtype=np.int16)
        for zoom in sorted({tile[0] for tile in self.receipts}, reverse=True):
            unresolved = np.flatnonzero(~np.isfinite(result))
            if not len(unresolved):
                break
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
                pixels = (coordinates[selected] - cell) * (
                    height.shape[1] - 1,
                    height.shape[0] - 1,
                )
                base = np.minimum(
                    np.floor(pixels).astype(int),
                    (height.shape[1] - 2, height.shape[0] - 2),
                )
                u, v = (pixels - base).T
                x, y = base.T
                result[unresolved[selected]] = (1 - v) * (
                    (1 - u) * height[y, x] + u * height[y, x + 1]
                ) + v * ((1 - u) * height[y + 1, x] + u * height[y + 1, x + 1])
                used_zoom[unresolved[selected]] = zoom
        if not np.isfinite(result).all():
            raise ValueError(
                f"DEM does not cover {int(np.count_nonzero(~np.isfinite(result)))} requested samples"
            )
        return result.reshape(shape), used_zoom.reshape(shape)

    def sample(self, latitude, longitude):
        return self.sample_with_zoom(latitude, longitude)[0]
