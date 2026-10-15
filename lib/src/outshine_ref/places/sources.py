"""Explicit bounded ZXY acquisition and lossless retention of provider semantics."""

import hashlib
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np


def tile_tuple(value):
    if (
        not isinstance(value, (tuple, list))
        or len(value) != 3
        or any(type(n) is not int for n in value)
        or not 0 <= value[0] <= 22
        or any(not 0 <= n < (1 << value[0]) for n in value[1:])
    ):
        raise ValueError("bounded integer Web-Mercator ZXY required")
    return tuple(value)


def tile_geography(points, extent, tile):
    zoom, x, y = tile_tuple(tile)
    if type(extent) is not int or not 1 <= extent <= 2**24:
        raise ValueError("positive bounded MVT extent required")
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
        raise ValueError("finite Nx2 MVT coordinates required")
    world = (points / extent + (x, y)) / (1 << zoom)
    longitude = (world[:, 0] * 360) % 360 - 180
    try:
        with np.errstate(over="raise", invalid="raise"):
            latitude = np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * world[:, 1]))))
    except FloatingPointError as error:
        raise ValueError("MVT latitude cannot be represented") from error
    return latitude, longitude


def covering_tiles(latitude, longitude, radius_m, zoom, *, max_tiles=4096):
    if (
        not np.isfinite((latitude, longitude, radius_m)).all()
        or abs(latitude) >= 85.05112878
        or abs(longitude) >= 180
        or not 0 <= radius_m <= 500000
        or type(zoom) is not int
        or not 0 <= zoom <= 22
        or type(max_tiles) is not int
        or not 1 <= max_tiles <= 65536
    ):
        raise ValueError("bounded geographic source window and tile budget required")
    side = 1 << zoom
    latitude_margin = np.rad2deg(radius_m / (6378137 * (1 - 6.6943799901413165e-3)))
    furthest_latitude = abs(latitude) + latitude_margin
    longitude_margin = (
        180
        if furthest_latitude >= 90
        else min(
            180,
            np.rad2deg(radius_m / (6378137 * np.cos(np.deg2rad(furthest_latitude)))),
        )
    )
    south, north = np.clip(
        (latitude - latitude_margin, latitude + latitude_margin),
        -85.05112878,
        85.05112878,
    )
    bounds = np.array(
        (
            (
                (longitude - longitude_margin + 180) / 360,
                (1 - np.arcsinh(np.tan(np.deg2rad(north))) / np.pi) / 2,
            ),
            (
                (longitude + longitude_margin + 180) / 360,
                (1 - np.arcsinh(np.tan(np.deg2rad(south))) / np.pi) / 2,
            ),
        )
    )
    lower, upper = np.floor(bounds * side).astype(int)
    lower[1], upper[1] = max(0, lower[1]), min(side - 1, upper[1])
    nx = min(side, int(upper[0] - lower[0] + 1))
    ny = int(upper[1] - lower[1] + 1)
    if nx * ny > max_tiles:
        raise ValueError("source tile budget exceeded before tile allocation")
    return tuple(
        (zoom, x % side, y)
        for y in range(int(lower[1]), int(upper[1]) + 1)
        for x in range(int(lower[0]), int(lower[0]) + nx)
    )


def _retain(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=path.parent, prefix=path.stem + ".", suffix=".tmp", delete=False
        ) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        try:
            os.link(temporary, path)
        except FileExistsError:
            if path.read_bytes() != data:
                raise ValueError(
                    "retained source differs at the same URI; use an explicit new snapshot"
                ) from None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def acquire(subject, root, *, fetch=False, max_bytes=16 * 1024**2, timeout_s=60):
    kind, tile, url = subject
    tile = tile_tuple(tile)
    if kind not in ("vector", "elevation") or not isinstance(url, str) or not url:
        raise ValueError("explicit vector/elevation source URI required")
    if (
        type(max_bytes) is not int
        or not 1 <= max_bytes <= 64 * 1024**2
        or not 0 < timeout_s <= 120
    ):
        raise ValueError("bounded source payload and timeout required")
    suffix = ".pbf" if kind == "vector" else ".webp"
    path = (
        Path(root)
        / ("openfreemap" if kind == "vector" else "mapterhorn")
        / (hashlib.sha256(url.encode()).hexdigest() + suffix)
    )
    if path.is_file():
        if not 0 < path.stat().st_size <= max_bytes:
            raise ValueError("retained source exceeds the payload budget or is empty")
        data, origin = path.read_bytes(), "existing-source-cache"
    elif fetch:
        encoded = subprocess.check_output(
            [
                "curl",
                "--silent",
                "--show-error",
                "--location",
                "--max-time",
                str(timeout_s),
                "--max-filesize",
                str(max_bytes),
                "--write-out",
                "\n%{http_code}",
                url,
            ]
        )
        data, separator, status = encoded.rpartition(b"\n")
        if not separator:
            raise ValueError("provider did not return an HTTP status")
        if status == b"404" and kind == "elevation":
            return {
                "kind": kind,
                "tile": list(tile),
                "url": url,
                "missing": True,
                "reason": "Preferred elevation provider returns HTTP 404",
            }
        if status != b"200" or not 0 < len(data) <= max_bytes:
            raise ValueError(
                "provider returned invalid status, size or an empty payload"
            )
        _retain(path, data)
        origin = "network"
    else:
        raise FileNotFoundError(str(path) + "; acquisition requires fetch=True")
    return {
        "kind": kind,
        "tile": list(tile),
        "url": url,
        "path": str(path),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "origin": origin,
    }


def receipt_bytes(receipt, *, max_bytes=16 * 1024**2):
    if receipt.get("missing"):
        raise ValueError("missing source has no payload")
    path = Path(receipt["path"])
    if not 0 < path.stat().st_size <= max_bytes:
        raise ValueError("source exceeds decode byte budget or is empty")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != receipt["sha256"]:
        raise ValueError("pinned source bytes changed")
    return data


def decode_vector(receipt):
    import mapbox_vector_tile

    if receipt["kind"] != "vector":
        raise ValueError("vector source receipt required")
    return mapbox_vector_tile.decode(
        receipt_bytes(receipt), default_options={"y_coord_down": True}
    )
