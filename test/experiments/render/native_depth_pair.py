"""Read only declared native camera/depth data from #1078's frozen diagnostics."""

import hashlib
import json
import struct

import numpy as np


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_pair(directory):
    metadata = json.loads((directory / "native-inputs.json").read_text())
    raw = (directory / "final.cull-audit").read_bytes()
    if raw[:8] != b"OSHCULL1" or len(raw) < 28:
        raise ValueError("Expected the frozen native camera-audit ABI")
    count, dropped, size, _ = struct.unpack_from("<IIIQ", raw, 8)
    if count != 60 or dropped or size != 400 or len(raw) != 28 + count * size:
        raise ValueError("Expected all original 60 camera records without losses")
    rows = list(struct.iter_unpack("<IIIIQQ3d3d16f256s", raw[28:]))
    if [r[0] for r in rows] != list(range(1, 61)):
        raise ValueError("Native camera frames are not the declared complete sequence")
    result = []
    hashes = {
        name: digest(directory / name)
        for name in ["native-inputs.json", "final.cull-audit"]
    }
    for row in rows[-2:]:
        frame = row[0]
        stated = metadata["per_frame"][frame - 1]
        clip = np.array(row[12:28], dtype=np.float64).reshape(4, 4).T
        cull = np.frombuffer(row[-1], dtype="<f4")
        if stated["frame"] != frame or stated["clip_from_relative_world"] != list(
            row[12:28]
        ):
            raise ValueError("Camera JSON differs from native recorded matrix words")
        if not np.array_equal(clip.T.ravel(), cull[32:48]):
            raise ValueError("Raster clip differs from the captured current cull clip")
        width, height = struct.unpack_from("<II", row[-1], 244)
        if (width, height) != (1280, 720):
            raise ValueError(
                "This frozen comparison requires the original 1280x720 extent"
            )
        path = directory / f"final.frame-{frame}.depth.f32"
        stated_depth = metadata["depths"][str(frame)]
        if (
            path.stat().st_size != width * height * 4
            or digest(path) != stated_depth["sha256"]
        ):
            raise ValueError("Depth does not match its original size/hash receipt")
        depth = np.fromfile(path, dtype="<f4").reshape(height, width)
        if not np.isfinite(depth).all() or ((depth < 0) | (depth > 1)).any():
            raise ValueError("Invalid native D32 reversed-Z values")
        result.append(
            {
                "frame": frame,
                "clip": clip,
                "pre_view": np.array(row[9:12]),
                "generation": (row[4], row[5]),
                "source_anchor": tuple(row[6:9]),
                "depth": depth,
            }
        )
        hashes[path.name] = digest(path)
    return result, hashes
