"""Prepare native full-camera light-list controls from retained source axes (#448)."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from local_light_grid import build_grid
from local_light_inputs import ROOT, Lights, camera, street_lights


def inputs(name, mixed, camera_name):
    lights, receipt = street_lights(
        spacing=4 if name == "dense" else 16,
        count={"dense": 8189, "small": 14}.get(name),
    )
    if name == "overlap":
        lights = Lights(
            np.tile(np.median(lights.positions, axis=0), (1021, 1)),
            np.full(1021, 128.0),
            np.tile([600, 390, 180], (1021, 1)),
            [f"authored-overlap/{i}" for i in range(1021)],
        )
    frame = camera(lights, camera_name)
    # Controls are centred in the camera so even the infinite tail is exercised.
    controls = frame.eye + np.array([-6, 10, 4994])[:, None] * frame.basis[2]
    lights.positions = (
        np.vstack((lights.positions, controls)).astype("<f4").astype(float)
    )
    lights.ranges = np.r_[lights.ranges, [32, 32, 32]]
    lights.colours = np.vstack((lights.colours, np.tile([600, 390, 180], (3, 1))))
    lights.source_ids += ["authored/behind-eye", "authored/near", "authored/tail"]
    packed = np.zeros((len(lights.positions), 4, 4), dtype="<f4")
    packed[:, 0, :3] = lights.colours
    packed[:, 0, 3] = 1
    packed[:, 1, :3] = lights.positions
    packed[:, 2, :3] = frame.basis[2]
    packed[:, 3, :2] = [np.cos(np.pi / 4), 1 / (np.cos(np.pi / 7) - np.cos(np.pi / 4))]
    sun = 0
    if mixed:
        packed[::3, 0, 3] = 2
        lights.ranges[5] = 0
        directional = [1, len(packed) - 4]
        lights.ranges[directional] = 0
        packed[directional, 0, 3] = 0
        packed[directional, 0, :3] = [[0.07, 0.1, 0.2], [0.4, 0.3, 0.2]]
        sun = directional[-1] + 1
    np.divide(1, lights.ranges, out=packed[:, 1, 3], where=lights.ranges > 0)
    receipt.update(scene=name, mixed_authored_kinds=mixed, source_ids=lights.source_ids)
    return lights, frame, packed, sun, receipt


def export(output, name, mixed, camera_name, index_budget):
    lights, frame, packed, sun, receipt = inputs(name, mixed, camera_name)
    output.mkdir(parents=True, exist_ok=True)
    packed.tofile(output / "lights.bin")
    lighting = np.zeros(336, dtype="<f4")
    lighting[:4] = [len(packed), 0, sun, int(sun > 0)]
    lighting[20:24] = (
        np.r_[frame.eye, 1] if frame.perspective else np.r_[-frame.basis[2], 0]
    )
    if sun:
        lighting[32:36] = [1, 1, 0, 0]
        lighting[36:40] = [1, 0, 0, 0]
        lighting[40:44] = [0, 0, 1, 0]
    lighting.tofile(output / "lighting.bin")
    probe = np.zeros((5, 4), dtype="<f4")
    probe[:3, :3] = frame.basis
    probe[3, :3] = frame.eye
    probe[4] = [*frame.half, 25, int(frame.perspective)]
    probe.tofile(output / "probe.bin")
    for mode in ("tiles", "log"):
        grid = build_grid(
            lights,
            frame,
            mode,
            index_budget=index_budget,
        )
        headers = grid.headers.copy()
        index_start = 4 + headers.size
        headers[:, 0] += index_start
        global_start = index_start + len(grid.indices)
        edge_start = global_start + len(grid.globals)
        words = np.r_[
            np.array([global_start, len(grid.globals), 4, edge_start], dtype="<u4"),
            headers.ravel(),
            grid.indices,
            grid.globals,
            grid.edges.astype("<f4").view("<u4"),
        ].astype("<u4")
        words.tofile(output / f"{mode}-lists.bin")
        block = np.zeros(16, dtype="<f4")
        block.view("<u4")[:4] = [*grid.extent(), len(grid.edges) - 1, grid.tile]
        block[4:8] = [frame.near, frame.depth_extent, 0, 0]
        block[8:12] = np.r_[frame.basis[2], -np.dot(frame.eye, frame.basis[2])]
        block[12:16] = [0, 0, frame.width, frame.height]
        block.tofile(output / f"{mode}-frame.bin")
        if mode == "tiles":
            block.tofile(output / "brute-frame.bin")
        receipt[mode] = {
            "grid": grid.stats,
            "list_bytes": words.nbytes,
            "frame_bytes": block.nbytes,
        }
    receipt.update(
        scope="Native direct-shading control over procedural receiver planes; not actual road surfaces, full framepipeline, texture/transparent composition or device acceptance.",
        fixture_scene=name,
        camera=camera_name,
        lights=len(packed),
        sun_world_index=sun - 1,
        profile_px=[frame.width, frame.height],
        planes_m=[25, 250, 500, 5000],
    )
    receipt["hashes"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.glob("*.bin")
    }
    (output / "inputs.json").write_text(json.dumps(receipt, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--scene", choices=["small", "street", "dense", "overlap"], default="street"
    )
    parser.add_argument(
        "--camera", choices=["street", "turn", "ortho"], default="street"
    )
    parser.add_argument("--mixed", action="store_true")
    parser.add_argument("--index-bytes", type=int, default=16 * 1024**2)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT) or args.index_bytes <= 0:
        parser.error("positive index budget and private external output required")
    export(args.output, args.scene, args.mixed, args.camera, args.index_bytes)


if __name__ == "__main__":
    main()
