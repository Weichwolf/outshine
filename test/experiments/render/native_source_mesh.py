"""Verified native source-mesh streams and original local source ownership."""

import hashlib
import json

import numpy as np


def read_capture(root, receipt, capture):
    """Verify every original stream; count units follow the native receipt."""
    specifications = receipt["captures"][capture]
    if set(specifications) != {
        "walls",
        "roofs",
        "wall_indices",
        "roof_indices",
        "colours",
        "metres",
    }:
        raise ValueError("complete native stream inventory required")
    arrays = {}
    for name, spec in specifications.items():
        stride = 24 if name in ("walls", "roofs") else 4
        if (
            type(spec["count"]) is not int
            or spec["count"] < 0
            or spec["count"] * stride != spec["bytes"]
        ):
            raise ValueError("source stream count/byte extent differs")
        data = (root / "mesh" / str(capture) / f"{name}.bin").read_bytes()
        if (
            len(data) != spec["bytes"]
            or hashlib.sha256(data).hexdigest() != spec["sha256"]
        ):
            raise ValueError("source stream changed")
        arrays[name] = np.frombuffer(data, dtype="<u4")
    return arrays


def load_source(root, *, allow_mixed_codes=False):
    receipt = json.loads((root / "mesh/receipt.json").read_text())
    metadata = json.loads((root / "input.json").read_text())
    arrays = read_capture(root, receipt, 0)
    walls = arrays["walls"].reshape(-1, 6)
    roofs = arrays["roofs"].reshape(-1, 6)
    for name, count in (("wall_indices", len(walls)), ("roof_indices", len(roofs))):
        if arrays[name].size and arrays[name].max() >= count:
            raise ValueError("source stream index outside its own vertices")
    words = np.concatenate((walls, roofs))
    indices = np.concatenate(
        (
            arrays["wall_indices"].reshape(-1, 3),
            arrays["roof_indices"].reshape(-1, 3) + len(walls),
        )
    ).astype(np.uint32)
    if (
        not len(indices)
        or indices.max() >= len(words)
        or not np.isfinite(words[:, :5].view("<f4")).all()
    ):
        raise ValueError("invalid source vertices")
    colour = np.ones((len(words), 4), dtype=np.float32)
    if len(arrays["colours"]):
        colour[: len(walls)] = arrays["colours"].view("<f4").reshape(-1, 4)
    metres = None
    if len(arrays["metres"]):
        metres = np.zeros((len(words), 2), np.float32)
        metres[: len(walls)] = arrays["metres"].view("<f4").reshape(-1, 2)
    uv = words[:, 3:5].view("<f4")
    code = np.where(uv[:, 0] < 0, uv[:, 0], np.floor(uv[:, 0] / 256))
    code_uniform = (code[indices] == code[indices[:, :1]]).all(1)
    if (
        (abs(code) > 32767).any()
        or (not allow_mixed_codes and not code_uniform.all())
        or not (words[indices, 5] == words[indices[:, :1], 5]).all()
        or not (colour[indices] == colour[indices[:, :1]]).all()
    ):
        raise ValueError(
            "this first capture requires flat codes/normals/colour per triangle"
        )
    palette, palette_index = np.unique(
        colour[indices[:, 0]], axis=0, return_inverse=True
    )
    if len(palette) > 65535:
        raise ValueError("bounded exact colour palette required")
    smooth = np.column_stack((np.mod(uv[:, 0], 256), uv[:, 1])).astype(np.float32)
    return {
        "positions": words[:, :3].view("<f4"),
        "indices": indices,
        "normal": words[indices[:, 0], 5],
        "code": np.where(code_uniform, code[indices[:, 0]], -32768).astype(np.int16),
        "code_uniform": code_uniform,
        "vertex_code": code.astype(np.int16),
        "palette": palette,
        "colour_index": palette_index.astype(np.uint16),
        "uv": smooth,
        "metres": metres,
        "domain": np.r_[
            np.zeros(len(arrays["wall_indices"]) // 3, np.uint8),
            np.ones(len(arrays["roof_indices"]) // 3, np.uint8),
        ],
        "metadata": metadata,
        "native_bytes": sum(s["bytes"] for s in receipt["captures"][0].values()),
    }


def face_sources(root, source):
    receipt = json.loads((root / "mesh/receipt.json").read_text())
    original = source["metadata"]["original_sources"]
    if receipt["original_sources"] != original:
        raise ValueError("original source identities differ")
    counts = [
        receipt["captures"][0][name]["count"]
        for name in ("wall_indices", "roof_indices")
    ]
    if any(type(n) is not int or n < 0 or n % 3 for n in counts):
        raise ValueError("complete source triangles required")
    count, roof_count = (n // 3 for n in counts)
    if count + roof_count != len(source["indices"]):
        raise ValueError("source triangle accounting differs")
    result = np.full(len(source["indices"]), -1, np.int32)
    for call in receipt["call_ranges"][0]:
        index = call["native_call"]["source"]
        if (
            type(index) is not int
            or not 0 <= index < len(original)
            or type(call["native_call"]["detail"]) is not int
            or call["native_call"]["detail"] != 0
            or call["native_call"].get("accepted") is not True
            or call["native_call"].get("openings") is not True
        ):
            raise ValueError("complete normal Fine source ranges required")
        for name, offset in (("wall_indices", 0), ("roof_indices", count)):
            begin, end = call[name]
            limit = receipt["captures"][0][name]["count"]
            if (
                type(begin) is not int
                or type(end) is not int
                or not 0 <= begin <= end <= limit
                or begin % 3
                or end % 3
                or (result[offset + begin // 3 : offset + end // 3] >= 0).any()
            ):
                raise ValueError("overlapping/misaligned original source range")
            result[offset + begin // 3 : offset + end // 3] = index
    if (result < 0).any():
        raise ValueError("incomplete original face ownership")
    return result
