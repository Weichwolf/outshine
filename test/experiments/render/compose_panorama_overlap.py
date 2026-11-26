"""Bake the same prepared surface into the handoff and the far colour/depth cube."""

import argparse
import json
from pathlib import Path

import numpy as np

from pack_far_panorama import FACES, write_product


def read_buffers(folder, side, mode):
    shapes = (("colour.rgba16f", "<f2", (6, side, side, 4)),
              ("depth.r32f", "<f4", (6, side, side)))
    result = []
    for name, dtype, shape in shapes:
        file = folder / name
        if file.stat().st_size != np.prod(shape) * np.dtype(dtype).itemsize:
            raise ValueError(f"{name} does not match the declared cube")
        result.append(np.memmap(file, mode=mode, dtype=dtype, shape=shape))
    return result


def overlap_weight(depth, axial_scale, begin, end, fade_m):
    axial = depth / axial_scale
    weight = np.clip(1 - (axial - end) / fade_m, 0, 1)
    return np.where(axial >= begin, weight * weight * (3 - 2 * weight), 0)


def aligned_metadata(panorama, surface):
    far = json.loads((panorama / "panorama.json").read_text())
    near = json.loads((surface / "panorama.json").read_text())
    alignment = ("origin_lon_lat", "anchor_eus_m", "face_px", "face_order",
                 "colour_encoding", "depth_encoding", "maximum_range_m",
                 "reference_exposure")
    for key in alignment:
        if far[key] != near[key]:
            raise ValueError(f"surface overlap must share the panorama's {key}")
    if near.get("panorama_reference_revision", far["revision"]) != far["revision"]:
        raise ValueError("surface overlap was lit from a different reference revision")
    far_light, near_light = far["surface_lighting"], near["surface_lighting"]
    if any(not np.allclose(far_light[key], near_light[key], rtol=1e-6, atol=1e-10)
           for key in ("to_sun_eus", "direct", "sky", "ground")):
        raise ValueError("surface overlap must share the panorama's lighting")
    return far, near


def validate_face(colour, depth, maximum_range_m, face):
    if not (np.isfinite(colour).all() and np.isfinite(depth).all()
            and np.all((depth >= 0) & (depth <= maximum_range_m))):
        raise ValueError(f"{face} has invalid surface samples")


def compose(panorama, surface, output, fade_m):
    far, near = aligned_metadata(panorama, surface)
    if far.get("baked_group_keys"):
        raise ValueError("compose the surface overlap before adding distant group coverage")
    side = far["face_px"]
    begin, end = far["axial_near_m"], far["axial_overlap_end_m"]
    if not (far["face_order"] == list(FACES) and 2 <= side <= 2048
            and 0 < near["axial_near_m"] <= begin < end
            and 0 < fade_m < far["maximum_range_m"] - end):
        raise ValueError("aligned clipped cubemaps and a positive, in-range fade are required")
    colour, depth = read_buffers(panorama, side, "c")
    surface_colour, surface_depth = read_buffers(surface, side, "r")
    coord = (np.arange(side, dtype=np.float32) + .5) * (2 / side) - 1
    axial_scale = np.sqrt(1 + coord[:, None] ** 2 + coord[None, :] ** 2)
    updated = []
    for index in range(len(FACES)):
        overlay, distance = surface_colour[index], surface_depth[index]
        validate_face(overlay, distance, far["maximum_range_m"], FACES[index])
        weight = overlap_weight(distance, axial_scale, begin, end, fade_m)
        touched = weight > 0
        scale = weight[touched, None]
        colour[index, ..., :3][touched] = (overlay[..., :3][touched] * scale
                                           + colour[index, ..., :3][touched] * (1 - scale))
        prefer_surface = weight >= .5
        depth[index][prefer_surface] = distance[prefer_surface]
        colour[index, ..., 3] = depth[index] > 0
        updated.append(int(touched.sum()))
    if not any(updated):
        raise ValueError("surface cube contains no samples in the handoff")
    far["surface_overlap_revision"] = near["revision"]
    write_product(output, far, colour, depth)
    print(json.dumps({"written_texels": dict(zip(FACES, updated)), "revision": far["revision"]}))


def replace_nearer(colour, depth, overlay, distance):
    take = (distance > 0) & ((depth == 0) | (distance < depth))
    colour[take] = overlay[take]
    depth[take] = distance[take]


def round_radial_depth(depth, translation_m, focal_px):
    if not np.isfinite(focal_px) or focal_px <= 0:
        raise ValueError("positive finite depth focal length required")
    rounded = (depth * .25).astype("<f2").astype("<f4") * 4
    covered = depth > 0
    stored = rounded[covered].astype(float)
    if not np.isfinite(stored).all() or np.any(stored <= 0):
        raise ValueError("binary16 depth would lose coverage or range")
    original = depth[covered].astype(float)
    if np.any(stored <= translation_m) or np.any(original <= translation_m):
        raise ValueError("translation sphere intersects sampled geometry")
    shift = abs(np.arcsin(translation_m / original) - np.arcsin(translation_m / stored))
    error_px = float(np.max(shift, initial=0)) * focal_px
    if error_px > .25:
        raise ValueError(f"depth quantization would shift geometry by {error_px:.3f} px")
    return rounded, error_px


def compose_standalone(panorama, surface, output, half_depth_focal_px):
    far, near = aligned_metadata(panorama, surface)
    side = far["face_px"]
    if not (far["face_order"] == list(FACES) and 2 <= side <= 2048
            and 0 < near["axial_near_m"] < far["axial_near_m"]):
        raise ValueError("standalone comparison requires a capture of the cube interior")
    colour, depth = read_buffers(panorama, side, "c")
    surface_colour, surface_depth = read_buffers(surface, side, "r")
    for index, face in enumerate(FACES):
        overlay, distance = surface_colour[index], surface_depth[index]
        validate_face(colour[index], depth[index], far["maximum_range_m"], face)
        validate_face(overlay, distance, far["maximum_range_m"], face)
        replace_nearer(colour[index], depth[index], overlay, distance)

    nearest_m = float(np.min(depth, where=depth > 0, initial=np.inf))
    if not np.isfinite(nearest_m):
        raise ValueError("standalone comparison requires at least one covered ray")
    domain_m = min(far["maximum_eye_translation_m"], nearest_m * .5)
    far["maximum_eye_translation_m"] = domain_m
    far["axial_near_m"] = 0
    far.pop("axial_overlap_end_m", None)
    if groups := far.pop("baked_group_keys", None):
        far["source_baked_group_keys"] = groups
    far["surface_overlap_revision"] = near["revision"]
    result = {"nearest_depth_m": nearest_m, "translation_domain_m": domain_m}
    if half_depth_focal_px is not None:
        depth, error_px = round_radial_depth(depth, domain_m, half_depth_focal_px)
        if np.any(depth > far["maximum_range_m"]):
            raise ValueError("binary16 depth exceeds the declared range")
        far["depth_precision"] = "binary16_scaled_4"
        far["depth_focal_bound_px"] = half_depth_focal_px
        result["depth_shift_bound_px"] = error_px
    write_product(output, far, colour, depth)
    print(json.dumps({**result, "revision": far["revision"]}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("panorama", type=Path)
    parser.add_argument("surface", type=Path)
    parser.add_argument("output", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--fade-m", type=float)
    mode.add_argument("--standalone", action="store_true")
    parser.add_argument("--half-depth-focal-px", type=float)
    args = parser.parse_args()
    if args.standalone:
        compose_standalone(args.panorama, args.surface, args.output, args.half_depth_focal_px)
    elif args.half_depth_focal_px is not None:
        parser.error("--half-depth-focal-px requires --standalone")
    else:
        compose(args.panorama, args.surface, args.output, args.fade_m)


if __name__ == "__main__":
    main()
