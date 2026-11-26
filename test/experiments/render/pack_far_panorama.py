"""Pack six native linear colour and depth captures into one far-world product."""

import argparse
import hashlib
import json
from pathlib import Path
from time import perf_counter

import numpy as np

from far_cubemap import DOWN, FORWARD, from_device_depth

FACES = ("px", "nx", "py", "ny", "pz", "nz")


def attachment(path, shape):
    expected = int(np.prod(shape))
    if path.stat().st_size != expected * np.dtype("<f4").itemsize:
        raise ValueError(f"{path}: invalid attachment size")
    values = np.fromfile(path, dtype="<f4").reshape(shape)
    if not np.isfinite(values).all():
        raise ValueError(f"{path}: nonfinite pixel")
    return values


def pack(capture, output, exposure=None, translation_m=0, overlap_end_m=None):
    began = perf_counter()
    manifest = json.loads((capture / "capture.json").read_text())
    side = manifest["face_px"]
    near, far = manifest["near_m"], manifest["far_m"]
    if (manifest["capture_version"] != 1 or not 2 <= side <= 2048 or not 0 < near < far
            or manifest["colour"] != "scene_linear_rgba_f32_le"
            or manifest["depth"] != "reversed_perspective_f32_le"
            or len(manifest["faces"]) != len(FACES)):
        raise ValueError("unsupported far panorama capture")
    if not 0 <= translation_m < near:
        raise ValueError("translation domain must be within the axial near plane")
    if overlap_end_m is not None and not near < overlap_end_m <= far:
        raise ValueError("overlap end must be past the capture near plane and within range")
    colour = np.empty((6, side, side, 4), dtype="<f2")
    depth = np.empty((6, side, side), dtype="<f4")
    maximum = np.finfo(np.float16).max
    for index, (name, face) in enumerate(zip(FACES, manifest["faces"])):
        if (face["name"] != name or face["forward_eus"] != FORWARD[index].tolist()
                or face["up_eus"] != (-DOWN[index]).tolist()):
            raise ValueError(f"{name}: mismatched capture orientation")
        image = attachment(capture / f"{name}-linear.f32", (side, side, 4))[:, ::-1]
        source_depth = attachment(capture / f"{name}-depth.f32", (side, side))
        radial = from_device_depth(source_depth, near, far)
        covered = (radial > 0) & (radial <= far)
        depth[index] = np.where(covered, radial, 0)
        np.clip(image[..., :3], 0, maximum, out=colour[index, ..., :3])
        colour[index, ..., 3] = covered

    if exposure is None:
        luminance = colour[..., :3].astype(np.float32) @ [0.2126, 0.7152, 0.0722]
        metered = float(np.median(luminance))
        exposure = .18 / metered if metered > 0 else 1.0
    if not np.isfinite(exposure) or exposure <= 0:
        raise ValueError("positive reference exposure required")
    product = {
        "version": 1,
        "representation": "colour_radial_depth_cubemap",
        "face_px": side,
        "face_order": FACES,
        "origin_lon_lat": manifest["origin_lon_lat"],
        "anchor_eus_m": manifest["eye_eus_m"],
        "axial_near_m": near,
        "maximum_range_m": far,
        "maximum_eye_translation_m": translation_m,
        "reference_exposure": exposure,
        "colour_encoding": "rgba16f_linear_le_a_opaque_coverage",
        "depth_encoding": "r32f_radial_metres_le_zero_without_geometry",
        "colour_bytes": colour.nbytes,
        "depth_bytes": depth.nbytes,
        "opaque_texels": [int(np.count_nonzero(face)) for face in depth],
        "declaration_sha256": manifest["declaration_sha256"],
        "source_declarations": manifest["source_declarations"],
    }
    if overlap_end_m is not None:
        product["axial_overlap_end_m"] = overlap_end_m
    revision = hashlib.sha256(json.dumps(product, sort_keys=True).encode("utf-8"))
    revision.update(colour)
    revision.update(depth)
    product["revision"] = revision.hexdigest()

    output.mkdir()
    colour.tofile(output / "colour.rgba16f")
    depth.tofile(output / "depth.r32f")
    (output / "panorama.json").write_text(json.dumps(product, indent=2, allow_nan=False) + "\n")
    print(json.dumps({
        "revision": product["revision"],
        "package_bytes": colour.nbytes + depth.nbytes,
        "opaque_texels": product["opaque_texels"],
        "preload_ms": manifest["preload_ms"],
        "capture_ms": manifest["capture_ms"],
        "capture_gpu_ms": manifest["gpu_submission_ms"],
        "pack_and_write_ms": (perf_counter() - began) * 1000,
    }))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--exposure", type=float, help="reference exposure from the world benchmark")
    parser.add_argument("--translation-domain-m", type=float, default=0,
                        help="declared radius for a bounded translated replay")
    parser.add_argument("--overlap-end-m", type=float,
                        help="axial end of the shared near-mesh/far-panorama coverage")
    options = parser.parse_args()
    pack(options.capture, options.output, options.exposure, options.translation_domain_m,
         options.overlap_end_m)


if __name__ == "__main__":
    main()
