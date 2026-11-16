"""Read existing OpenFreeMap/Mapterhorn tiles from the native source cache."""

import hashlib
from pathlib import Path
import struct

from .sources import tile_tuple


def cached_tile(root, tile, *, revision=None):
    """Match SourceIdentity.cpp ContentKey; never fetch or alter the cache."""
    tile = tile_tuple(tile)
    vector = revision is not None
    dataset = "openfreemap.openmaptiles" if vector else "mapterhorn.terrarium"
    endpoint = (f"https://tiles.openfreemap.org/planet/{revision}/{{z}}/{{x}}/{{y}}.pbf"
                if vector else "https://tiles.mapterhorn.com/{z}/{x}/{y}.webp")

    def text(value):
        data = value.encode()
        return struct.pack("<I", len(data)) + data

    data = text("raw-source-content-2") + text(dataset) + struct.pack("<I", 2 if vector else 1)
    data += b"".join(text(v) for v in (revision or "", "vector" if vector else "elevation",
                                    "/".join(map(str, tile)), endpoint, ""))
    path = Path(root) / hashlib.sha256(data).hexdigest()
    if not 0 < path.stat().st_size <= 64 * 1024**2:
        raise ValueError("bounded nonempty native source tile required")
    payload = path.read_bytes()
    return payload, {"tile": tile, "cache_key": path.name, "bytes": len(payload),
                     "sha256": hashlib.sha256(payload).hexdigest(), "endpoint": endpoint,
                     "path": str(path), "kind": "vector" if vector else "elevation",
                     "origin": "existing-native-source-cache"}
