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


def compose(panorama, surface, output, fade_m):
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
        if not (np.isfinite(overlay).all() and np.isfinite(distance).all()
                and np.all((distance >= 0) & (distance <= far["maximum_range_m"]))):
            raise ValueError(f"{FACES[index]} has invalid surface samples")
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("panorama", type=Path)
    parser.add_argument("surface", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--fade-m", type=float, required=True)
    args = parser.parse_args()
    compose(args.panorama, args.surface, args.output, args.fade_m)


if __name__ == "__main__":
    main()
