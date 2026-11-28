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

    def _tile_heights(self, tile, fraction):
        raster = self.raster(tile)
        height, width = raster.shape
        pixels = fraction * (width, height) - 0.5
        clamped = np.clip(pixels, 0, (width - 1, height - 1))
        base = np.minimum(np.floor(clamped).astype(int), (width - 2, height - 2))
        u, v = (clamped - base).T
        x, y = base.T
        result = (1 - v) * ((1 - u) * raster[y, x] + u * raster[y, x + 1]) + v * (
            (1 - u) * raster[y + 1, x] + u * raster[y + 1, x + 1]
        )
        edge = np.any(pixels != clamped, axis=1)
        if edge.any():
            result[edge] = self._edge_heights(
                tile, raster.shape, pixels[edge], result[edge]
            )
        return result

    def _edge_heights(self, tile, shape, pixels, fallback):
        zoom, tile_x, tile_y = tile
        side = 1 << zoom
        height, width = shape
        size = np.array((width, height))
        base = np.floor(pixels).astype(int)
        fraction = pixels - base
        values = np.zeros(len(base))
        total = np.zeros(len(base))
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
            corner = np.array((dx, dy))
            cell, pixel = np.divmod(base + corner, size)
            weight = np.prod(np.where(corner, fraction, 1 - fraction), axis=1)
            for east, south in np.unique(cell, axis=0):
                neighbor = zoom, (tile_x + int(east)) % side, tile_y + int(south)
                if neighbor not in self.receipts:
                    continue
                raster = self.raster(neighbor)
                if raster.shape != shape:
                    continue
                hit = (cell[:, 0] == east) & (cell[:, 1] == south)
                x, y = pixel[hit].T
                values[hit] += raster[y, x] * weight[hit]
                total[hit] += weight[hit]
        return np.divide(values, total, out=fallback, where=total > 0)

    def _sample_covered_boundaries(self, world, zooms, result, used_zoom):
        for zoom in zooms:
            pending = np.flatnonzero(~np.isfinite(result))
            if not len(pending):
                break
            side = 1 << zoom
            xy = world[pending] * side
            boundary = np.rint(xy)
            tolerance = 4 * np.finfo(xy.dtype).eps * side
            shared = np.abs(xy - boundary) <= tolerance
            xy = np.where(shared, boundary, xy)
            base = np.floor(xy).astype(int)
            on_boundary = shared.any(axis=1)
            for offset in np.array(((0, 0), (-1, 0), (0, -1), (-1, -1))):
                eligible = on_boundary & ~np.isfinite(result[pending])
                eligible &= (shared | (offset == 0)).all(axis=1)
                positions = np.flatnonzero(eligible)
                cells, groups = np.unique(
                    base[positions] + offset, axis=0, return_inverse=True
                )
                for i, cell in enumerate(cells):
                    tile = zoom, int(cell[0]) % side, int(cell[1])
                    if tile in self.receipts:
                        selected = positions[groups == i]
                        result[pending[selected]] = self._tile_heights(
                            tile, xy[selected] - cell
                        )
                        used_zoom[pending[selected]] = zoom

    def sample_with_zoom(self, latitude, longitude, *, allow_missing=False):
        """Return the finest available height and zoom, with NaN and -1 for allowed misses."""
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
        zooms = sorted({tile[0] for tile in self.receipts}, reverse=True)
        for zoom in zooms:
            unresolved = np.flatnonzero(~np.isfinite(result))
            if not len(unresolved):
                break
            side = 1 << zoom
            coordinates = world[unresolved] * side
            coordinates[:, 0] %= side
            coordinates[:, 1] = np.clip(
                coordinates[:, 1], 0, np.nextafter(float(side), 0)
            )
            integer_cells = np.floor(coordinates).astype(int)
            if np.all(integer_cells == integer_cells[:1]):
                cells, inverse = integer_cells[:1], None
            else:
                cells, inverse = np.unique(integer_cells, axis=0, return_inverse=True)
            for index, cell in enumerate(cells):
                tile = zoom, int(cell[0]), int(cell[1])
                if tile not in self.receipts:
                    continue
                selected = (
                    np.arange(len(coordinates))
                    if inverse is None
                    else np.flatnonzero(inverse == index)
                )
                result[unresolved[selected]] = self._tile_heights(
                    tile, coordinates[selected] - cell
                )
                used_zoom[unresolved[selected]] = zoom
        self._sample_covered_boundaries(world, zooms, result, used_zoom)
        if not allow_missing and not np.isfinite(result).all():
            raise ValueError(
                f"DEM does not cover {int(np.count_nonzero(~np.isfinite(result)))} requested samples"
            )
        return result.reshape(shape), used_zoom.reshape(shape)

    def sample(self, latitude, longitude):
        return self.sample_with_zoom(latitude, longitude)[0]
