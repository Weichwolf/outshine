"""Read fixed instrumented terrain observations without changing their frame or words."""

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from outshine_ref.contracts import immutable
from terrain_material_grid import frozen

LAYOUT = (
    "pixel",
    "world_m",
    "world_dx",
    "world_dy",
    "surface_normal_and_facing",
    "base_shading_normal",
    "shading_normal",
    "rock_other_class_slope",
    "edge_halo_reach_across_m",
    "albedo_roughness",
    "bump_dxdy_uv",
    "position_half_footprint_m",
    "light_up",
    "which_runner_palettehash_shader",
)


@dataclass(frozen=True, slots=True)
class Capture:
    metadata: object
    points: np.ndarray
    palette_words: np.ndarray
    palette: np.ndarray
    input_sha256: str


def load(path):
    path = Path(path)
    if not 0 < path.stat().st_size <= 1024**2:
        raise ValueError("bounded native terrain fragment input required")
    raw = path.read_bytes()
    payload = json.loads(raw)
    if (
        payload.get("schema") != "outshine.terrain-fragment-inputs/1"
        or payload.get("record_layout")
        != [{"name": name, "lanes": 4} for name in LAYOUT]
        or not isinstance(payload.get("limitations"), list)
        or not payload["limitations"]
    ):
        raise ValueError("known instrumented native observation layout/scope required")
    words = payload.get("palette_words_u32")
    if (
        not isinstance(words, list)
        or len(words) != 319
        or any(type(w) is not int or not 0 <= w < 2**32 for w in words)
    ):
        raise ValueError("all actual native palette UInt32 words required")
    words = np.asarray(words, dtype="<u4")
    if hashlib.sha256(words.tobytes()).hexdigest() != payload["palette_sha256"]:
        raise ValueError("captured native palette hash mismatch")
    if words[0] != 14 or words[3] != 199:
        raise ValueError("current captured palette layout required")
    records = payload.get("points")
    if not isinstance(records, list) or not 1 <= len(records) <= 256:
        raise ValueError("bounded actual native point records required")
    points = np.asarray(records, dtype=np.float64)
    if points.shape != (len(records), 56) or not np.isfinite(points).all():
        raise ValueError("complete finite native fragment records required")
    if np.any(points[:, 28:31] < 0) or np.any(points[:, 28:31] > 1):
        raise ValueError(
            "current nonnegative clamped material mixture weights required"
        )
    observations = points[:, :52]
    if (
        observations.tobytes()
        != observations.astype(np.float32).astype(np.float64).tobytes()
    ):
        raise ValueError("original binary32 observations must be preserved")
    for row in records:
        if (
            any(type(v) is not int for v in row[52:])
            or any(not -(2**31) <= v < 2**31 for v in row[52:54])
            or any(not 0 <= v < 2**32 for v in row[54:])
        ):
            raise ValueError(
                "actual signed selection and unsigned palette/shader words required"
            )
    fnv = 2166136261
    for word in words:
        fnv = ((fnv ^ int(word)) * 16777619) & 0xFFFFFFFF
    if np.any(points[:, 54] != fnv) or any(
        str(int(v)) not in payload["shader_sources"] for v in points[:, 55]
    ):
        raise ValueError("per-fragment palette and shader binding mismatch")
    profile = payload["profile"]
    if (
        np.any(points[:, :2] < 0)
        or np.any(points[:, 0] >= profile["width_px"])
        or np.any(points[:, 1] >= profile["height_px"])
    ):
        raise ValueError("captured fragment outside its declared frame")
    if len({tuple(xy) for xy in points[:, :2]}) != len(points):
        raise ValueError("ambiguous repeated native pixel")
    if (
        payload["opaque_depth_equal_unique_points"] < len(points)
        or not 0 <= payload["frame_index"] < profile["fps"]
    ):
        raise ValueError("actual bounded final-frame observation binding required")
    metadata = {
        k: v for k, v in payload.items() if k not in ("points", "palette_words_u32")
    }
    return Capture(
        immutable(metadata),
        frozen(points),
        frozen(words),
        frozen(words.view("<f4").astype(np.float64)),
        hashlib.sha256(raw).hexdigest(),
    )
