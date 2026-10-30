"""Independent corner and typed binary32 controls for conditional box envelopes."""

import hashlib
import json
from pathlib import Path

import numpy as np
from capture_box_envelope import box_envelope

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-capture-operands"


def submitted_inputs(data=DATA):
    manifest = json.loads((data / "manifest.json").read_text())
    path = data / "operands.npz"
    if hashlib.sha256(path.read_bytes()).hexdigest() != manifest["npz_sha256"]:
        raise ValueError("Submitted operand archive differs from the frozen receipt")
    with np.load(path, allow_pickle=False) as archive:
        arrays = {key: archive[key] for key in manifest["arrays"]}
    for key, value in arrays.items():
        record = manifest["arrays"][key]
        if (
            value.dtype.str != record["dtype"]
            or list(value.shape) != record["shape"]
            or hashlib.sha256(value.tobytes()).hexdigest() != record["sha256"]
        ):
            raise ValueError("Submitted array bytes differ from the frozen receipt")
    uniform = arrays["geometry_view_uniform_f32"]
    model = np.frombuffer(arrays["gpu_placement_bytes"], "<f4", count=16)
    operands = (
        model.reshape(4, 4, order="F"),
        uniform[:16].reshape(4, 4, order="F"),
        uniform[48:51],
    )
    positions = arrays["vertex_positions_f32"]
    if not np.isfinite(positions).all() or np.any(
        (positions != 0) & (abs(positions) < np.finfo(np.float32).tiny)
    ):
        raise ValueError("Complete submitted positions violate the #990 domain")
    return manifest, arrays, operands


def corner_envelope(lower, upper, model, view, shift, **options):
    """Expand all eight corners and union their independent point envelopes."""
    choices = np.array(
        [[x, y, z] for x in (False, True) for y in (False, True) for z in (False, True)]
    )
    corners = np.where(choices, upper[:, None], lower[:, None]).reshape(-1, 3)
    options.pop("bounds_most", None)
    points = box_envelope(
        corners, corners, model, view, shift, bounds_most=8 * len(lower), **options
    )
    result = points.copy()
    for key in ("lower", "ideal_lower"):
        result[key] = points[key].reshape(-1, 8, 4).min(1)
    for key in ("upper", "ideal_upper", "error_upper"):
        result[key] = points[key].reshape(-1, 8, 4).max(1)
    return result


def typed_projection(positions, model, view, shift, *, flat):
    """CPU nearest-even binary32 multiply/add, explicitly without fused operations.

    This is a conditional expression opponent, never a production GPU replay.
    """
    matrix = model.copy()
    if flat:
        matrix[:3, 3] = np.add(matrix[:3, 3], shift, dtype=np.float32)
    points = np.column_stack((positions, np.ones(len(positions), np.float32)))
    for index, current in enumerate((matrix, view)):
        result = np.zeros_like(points)
        for row in range(4):
            for column in range(4):
                product = np.multiply(
                    current[row, column], points[:, column], dtype=np.float32
                )
                result[:, row] = np.add(result[:, row], product, dtype=np.float32)
        if index == 0 and not flat:
            result[:, :3] = np.add(result[:, :3], shift, dtype=np.float32)
        points = result
    return points
