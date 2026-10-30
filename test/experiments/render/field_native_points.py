"""Read the existing radiance-world raw files and actual native camera operands.

This offline reference adapter establishes byte/camera identity only. The caller
owns verified frame/source/material/light provenance. Geometry coverage and depth
error remain unknown until independently qualified; a valid hash is no such proof.
The compact observer report is not a full camera-basis or lighting-state proof;
the native producer and the held job retain those obligations.
"""

import hashlib
import json
from dataclasses import replace

import numpy as np
from field_external_depth import PointSamples, SampleBinding, validate


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)


def vector(value):
    result = np.asarray(value, dtype=float)
    if result.shape != (3,) or not np.isfinite(result).all():
        raise ValueError("Finite native three-component camera operand required")
    return result


def capture_description(value, texels_most):
    if not isinstance(value, dict) or value.get("format") != "reverse_z_0_1_float32":
        raise ValueError("Actual native source_depth description required")
    width, height = value.get("width_px"), value.get("height_px")
    if (
        value.get("orthographic") is not True
        or any(type(n) is not int or n < 1 for n in (width, height))
        or width * height > texels_most
        or value.get("bytes") != width * height * 4
    ):
        raise ValueError("Bounded native orthographic Depth32F raster required")
    camera = {
        key: vector(value.get(key)).tolist()
        for key in ("eye_m", "right", "up", "forward")
    }
    axes = np.asarray([camera[k] for k in ("right", "up", "forward")])
    scalars = [
        value.get(k) for k in ("near_m", "far_m", "x_mag_m", "y_mag_m", "yfov_rad")
    ]
    if (
        not all(type(x) in (float, int) and np.isfinite(x) for x in scalars)
        or not 0 < scalars[0] < scalars[1]
        or min(scalars[2:4]) <= 0
        or scalars[4] != 0
        or not np.allclose(axes @ axes.T, np.eye(3), rtol=0, atol=1e-9)
    ):
        raise ValueError(
            "Unmodified orthonormal rays, finite magnitudes and ordered clips required"
        )
    camera.update(
        {
            key: float(value[key])
            for key in ("near_m", "far_m", "x_mag_m", "y_mag_m", "yfov_rad")
        }
    )
    camera.update(
        {key: value[key] for key in ("format", "orthographic", "width_px", "height_px")}
    )
    camera["sample_locations"] = "native pixel centres"
    return camera


def observer_description(value):
    if not isinstance(value, dict) or type(value.get("orthographic")) is not bool:
        raise ValueError("Actual native lighting_observer required")
    eye, forward = vector(value.get("eye_m")), vector(value.get("forward"))
    if abs(float(forward @ forward) - 1) > 1e-9:
        raise ValueError("A unit actual observer direction is required")
    if value["orthographic"]:
        return {"orthographic": True, "forward": forward.tolist()}
    return {"orthographic": False, "eye_m": eye.tolist()}


def native_binding(record, provenance, *, texels_most=262144):
    if type(texels_most) is not int or texels_most < 1:
        raise ValueError("Positive native point capacity required")
    if (
        not isinstance(record, dict)
        or not isinstance(provenance, SampleBinding)
        or record.get("field_asset_key") not in provenance.sources
    ):
        raise ValueError(
            "The held complete source field product must match field_asset_key"
        )
    capture = capture_description(record.get("source_depth"), texels_most)
    observer = observer_description(record.get("lighting_observer"))
    return replace(
        provenance, capture=canonical(capture), pbr_view=canonical(observer)
    ), capture


def read_file(path, size, expected):
    if (
        not isinstance(expected, str)
        or len(expected) != 64
        or path.stat().st_size != size
    ):
        raise ValueError("Pinned native raw file size/SHA256 required")
    with path.open("rb") as stream:
        raw = np.fromfile(stream, dtype="<f4", count=size // 4)
        trailing = stream.read(1)
    if trailing or raw.nbytes != size or hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError("Native raw payload changed or differs from its pin")
    raw.setflags(write=False)
    return raw


def read_native_points(
    colour_path,
    depth_path,
    record,
    provenance,
    *,
    colour_sha256,
    depth_sha256,
    texels_most=262144,
):
    """Read full raw source points; keep their entire domain unqualified.

    source_readback_bytes is work telemetry and may be zero for a warm held
    result. It is not the length of the retained payload. Use source_depth's
    actual raster and the explicit raw file pins. Never substitute query-camera
    operands for source_depth or lighting_observer, or normalize either camera.
    """
    binding, capture = native_binding(record, provenance, texels_most=texels_most)
    binding = replace(binding, sample_revision=(colour_sha256, depth_sha256))
    shape = capture["height_px"], capture["width_px"]
    size = shape[0] * shape[1]
    rgba = read_file(colour_path, size * 16, colour_sha256).reshape(*shape, 4)
    z = read_file(depth_path, size * 4, depth_sha256).reshape(shape)
    result = PointSamples(
        binding,
        rgba,
        z,
        np.broadcast_to(np.zeros((), np.float64), shape),
        np.broadcast_to(np.zeros((), bool), shape),
    )
    validate(result, texels_most)
    return result
