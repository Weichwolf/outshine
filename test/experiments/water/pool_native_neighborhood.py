"""Read the pinned original square page window into one explicit WGS84 reference."""

import hashlib
import json
import tarfile
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from outshine_ref.water.geodetic import local_points
from terrain_grid_lod import triangles

FIXTURE = (
    Path(__file__).parents[2] / "outshine/data/terrain/pool-contact-neighborhood.tar.xz"
)


@dataclass(frozen=True)
class Neighborhood:
    vertices: np.ndarray
    faces: np.ndarray
    heights: np.ndarray
    geographic: np.ndarray
    manifest: dict
    report: dict


def _files(path, metadata):
    archive = path.read_bytes()
    if (
        len(archive) > 128 * 1024
        or hashlib.sha256(archive).hexdigest() != metadata["archive_sha256"]
    ):
        raise ValueError("bounded original neighborhood archive hash required")
    with tarfile.open(path, "r:xz") as source:
        members = source.getmembers()
        if (
            not 3 <= len(members) <= 33
            or len({m.name for m in members}) != len(members)
            or any(
                not m.isfile() or Path(m.name).name != m.name or m.size > 16384
                for m in members
            )
            or sum(m.size for m in members) > 128 * 1024
        ):
            raise ValueError("bounded named neighborhood files required")
        return {m.name: source.extractfile(m).read() for m in members}


def load_neighborhood(path=FIXTURE):
    path = Path(path)
    metadata = json.loads(path.with_suffix(".json").read_text())
    files = _files(path, metadata)
    manifest = json.loads(files["input.json"])
    window = manifest["window_zoom_x0_x1_y0_y1"]
    zoom, x0, x1, y0, y1 = window
    if not 0 <= zoom <= 23 or not (0 <= x0 <= x1 < 2**zoom and 0 <= y0 <= y1 < 2**zoom):
        raise ValueError("valid original tile window required")
    if x1 - x0 != y1 - y0 or not 1 <= (x1 - x0 + 1) ** 2 <= 16:
        raise ValueError("bounded square original tile window required")
    expected = {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}
    if len(manifest["rows"]) != len(expected) or metadata["pages"] != len(expected):
        raise ValueError("complete original neighborhood required")
    for name, reference in (
        ("asset_key", "source_asset_key"),
        ("stored_sha256", "source_stored_sha256"),
        ("native_sha256", "source_native_sha256"),
    ):
        if manifest[name] != metadata[reference]:
            raise ValueError("neighborhood provenance does not match original package")
    side = (x1 - x0 + 1) * 32 + 1
    heights = np.full((side, side), np.nan, dtype="<f4")
    seen = np.zeros_like(heights, dtype=bool)
    required = {"input.json"}
    duplicates = 0
    for page in manifest["rows"]:
        row = page["row"]
        tile = row["x"], row["y"]
        if tile not in expected or row["zoom"] != zoom:
            raise ValueError("each original tile must occur exactly once")
        expected.remove(tile)
        if (
            row["side"] != 33
            or row["postings"] != 33
            or row["halo"] != 1
            or row["nodes"] != 1225
        ):
            raise ValueError("original 33-posting page layout required")
        for kind in ("height", "fractions"):
            name = page[kind + "_file"]
            required.add(name)
            if (
                name not in files
                or hashlib.sha256(files[name]).hexdigest() != page[kind + "_sha256"]
            ):
                raise ValueError("original neighborhood file hash mismatch")
        raw = files[page["height_file"]]
        fractions = np.fromstring(files[page["fractions_file"]].decode(), sep="\n")
        if len(raw) != 4900 or not np.array_equal(fractions, np.arange(33) / 32):
            raise ValueError(
                "complete native words and uniform posting fractions required"
            )
        field = np.frombuffer(raw, dtype="<f4").reshape(35, 35)
        if not np.isfinite(field).all():
            raise ValueError("finite original neighborhood heights required")
        y, x = (row["y"] - y0) * 32, (row["x"] - x0) * 32
        target, known = heights[y : y + 33, x : x + 33], seen[y : y + 33, x : x + 33]
        core = field[1:34, 1:34]
        if np.any(known & (target.view("<u4") != core.view("<u4"))):
            raise ValueError("shared original height words disagree")
        duplicates += int(known.sum())
        target[~known] = core[~known]
        known[:] = True
    if expected or not seen.all() or required != files.keys():
        raise ValueError("exact complete neighborhood file set required")
    height_bytes = len(manifest["rows"]) * 4900
    if metadata["height_bytes"] != height_bytes:
        raise ValueError("original neighborhood height byte count mismatch")
    x, y = np.meshgrid(x0 + np.arange(side) / 32, y0 + np.arange(side) / 32)
    geographic = np.column_stack(
        (
            x.ravel() / 2**zoom * 360 - 180,
            np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * y.ravel() / 2**zoom)))),
        )
    )
    vertices = local_points(manifest["anchor_lon_lat"], geographic, heights.ravel())
    return Neighborhood(
        vertices,
        triangles(side, 1).astype(np.int64),
        heights,
        geographic,
        manifest,
        {
            "pages": len(manifest["rows"]),
            "height_bytes": height_bytes,
            "shared_original_height_occurrences": duplicates,
            "archive_sha256": metadata["archive_sha256"],
            "geometry_scope": "Decoded original height words in the existing WGS84 Double reference, with explicit NW-SE diagonals; not native vertex/shader/Float32 equivalence.",
        },
    )
