"""Prepare a private world-cell raster control for the existing native probe (#448)."""

import argparse
import dataclasses
import hashlib
import importlib.util
import json
from pathlib import Path

import numpy as np
from export_local_light_native import export_control, inputs
from local_light_inputs import ROOT
from local_light_world import POSITION_CELL_M, build_world, cell_positions


def shaders(raster_root, output):
    script = raster_root / "test/experiments/render/local_light_native_shaders.py"
    spec = importlib.util.spec_from_file_location("raster_source", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    receipt = module.prepare_shaders(raster_root, output)
    subject = output / "localLightSubject.glsl"
    text = subject.read_text()
    old = "    Light light = emitters[lightIndex];"
    if text.count(old) != 1:
        raise ValueError("re-audit emitter decoding before preparing world controls")
    subject.write_text(text.replace(old, "    Light light = worldLightAt(lightIndex);"))
    base = (raster_root / "test/experiments/render/local_light_native.frag").read_text()
    start, end = (
        base.index("uvec2 lightSpan(vec3 p) {"),
        base.index("uint lightIndexAt("),
    )
    base = base[:start] + '#include "local_light_world.glsl"\n\n' + base[end:]
    start, end = base.index("  vec3 p = probe.eye.xyz"), base.index("  uint family =")
    base = (
        base[:start]
        + """  worldView();
  vec3 p = worldForward * probe.control.z +
           worldRight * (ndc.x * probe.control.x * scale) +
           probe.up.xyz * (ndc.y * probe.control.y * scale);
  vec3 n = -worldForward;
"""
        + base[end:]
    )
    tangent = "vec3(0.04), 1.0, vec3(0.0), probe.right.xyz,"
    if base.count(tangent) != 1:
        raise ValueError("re-audit receiver tangent before preparing world controls")
    base = base.replace(tangent, "vec3(0.04), 1.0, vec3(0.0), worldRight,")
    (output / "world.frag").write_text(base)
    receipt.update(
        world_direct_shader_sha256=hashlib.sha256(subject.read_bytes()).hexdigest(),
        base_probe_shader_sha256=hashlib.sha256(
            (
                raster_root / "test/experiments/render/local_light_native.frag"
            ).read_bytes()
        ).hexdigest(),
        world_probe_shader_sha256=hashlib.sha256(base.encode()).hexdigest(),
        world_changes="World-cell lookup and original-ID cell-local emitter rebasing; private receiver/camera scaffold. Original BRDF, point/spot attenuation and original-ID shadow ownership unchanged. No production binding change.",
    )
    return receipt


def export_world(output, scene, widths, methods, translation, index_bytes):
    _, frame, packed, sun, receipt = inputs(scene, True, "street")
    worlds = [build_world(packed, width, index_bytes, translation) for width in widths]
    frame = dataclasses.replace(frame, eye=frame.eye + translation)
    cells, local = cell_positions(frame.eye[None], POSITION_CELL_M)
    camera_cell, camera_local = cells[0], local[0]
    relative = dataclasses.replace(frame, eye=np.zeros(3))
    if not np.array_equal(worlds[0].local_lights, worlds[1].local_lights):
        raise AssertionError("lookup width must not change the shared emitter encoding")
    export_control(output, worlds[0].local_lights, relative, sun)
    for name, method, world, width in zip(("tiles", "log"), methods, worlds, widths):
        words, stats = world.pack(method)
        words.tofile(output / f"{name}-lists.bin")
        block = np.zeros(16, dtype="<f4")
        block.view("<u4")[:4] = [
            *camera_cell.view("<u4"),
            int(method == "sorted") | (stats["pose_word_offset"] << 1),
        ]
        block[4:8] = [POSITION_CELL_M, *camera_local]
        block[8:12] = [7.25, -3.5, 0.125, 0.2]
        block[12:16] = [width, 0, frame.width, frame.height]
        block.tofile(output / f"{name}-frame.bin")
        if name == "tiles":
            block.tofile(output / "brute-frame.bin")
        receipt[name] = stats
    receipt.update(
        scene=scene,
        widths_m=list(widths),
        position_cell_m=POSITION_CELL_M,
        methods=list(methods),
        translation_m=translation.tolist(),
        original_world_index_order_preserved=True,
        camera_world_m=frame.eye.tolist(),
        camera_cell=camera_cell.tolist(),
        camera_local_m=camera_local.tolist(),
        receiver_planes_m=[25, 250, 500, 5000],
        scope="Private camera-relative receiver planes and cell-local emitters. Sorted/hash lookup comparison, optional prescribed camera translation/rotation shares one GPU upload. Not actual world geometry, runtime residency/update/publication, general FP32 culling, transparent composition or device acceptance.",
    )
    receipt["hashes"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.glob("*.bin")
    }
    (output / "world-inputs.json").write_text(json.dumps(receipt, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--scene", choices=["small", "street", "dense", "overlap"], default="small"
    )
    parser.add_argument(
        "--width", nargs=2, type=int, choices=[16, 32, 64, 128], default=[32, 32]
    )
    parser.add_argument(
        "--methods", nargs=2, choices=["hash", "sorted"], default=["hash", "sorted"]
    )
    parser.add_argument("--index-bytes", type=int, default=16 * 1024**2)
    parser.add_argument("--translation", nargs=3, type=float, default=[0, 0, 0])
    parser.add_argument("--raster-root", type=Path)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    export_world(
        args.output,
        args.scene,
        args.width,
        args.methods,
        np.asarray(args.translation),
        args.index_bytes,
    )
    if args.raster_root:
        directory = args.output / "shaders"
        receipt = shaders(args.raster_root, directory)
        (directory / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
