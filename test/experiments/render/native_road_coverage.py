"""Measure road primitive depth coverage on a fixed native scene, not streaming cost."""

import argparse
import json
import lzma
import os
import re
import shutil
import sys
from pathlib import Path

import numpy as np
from native_visibility_cost import load
from native_visibility_data import digest, extract
from native_visibility_pipeline import build, execute

ROOT = Path(__file__).resolve().parents[3]
GEOMETRY = ROOT / "test/outshine/data/render/visibility/selected-native.tar.xz"
FIXTURE = ROOT / "test/outshine/data/render/road_coverage"


def prepare(output):
    manifest = json.loads((FIXTURE / "manifest.json").read_text())
    packed = FIXTURE / "scene-depth.f32.xz"
    if (
        manifest["schema"] != 1
        or manifest["frame_shape"] != [1280, 720]
        or digest(GEOMETRY) != manifest["existing_geometry_fixture_sha256"]
        or digest(packed) != manifest["compressed_sha256"]
    ):
        raise ValueError("road coverage input differs from its manifest")
    decoder = lzma.LZMADecompressor()
    depth = decoder.decompress(packed.read_bytes(), max_length=1280 * 720 * 4 + 1)
    if (
        len(depth) != 1280 * 720 * 4
        or not decoder.eof
        or decoder.unused_data
        or not np.isfinite(np.frombuffer(depth, dtype="<f4")).all()
    ):
        raise ValueError("invalid scene depth")
    source = output / "geometry"
    extract(GEOMETRY, source)
    snapshot = load(source)
    selected = snapshot.jobs[:, 1] == manifest["road_batch"]
    jobs = snapshot.jobs[selected]
    models = snapshot.models[selected]
    if len(jobs) != manifest["road_jobs"] or not np.all(models == models[0]):
        raise ValueError("road jobs or placement differ from the capture")
    indices = np.concatenate(
        [snapshot.indices[int(job[2]) : int(job[2]) + int(job[3])] for job in jobs]
    )
    triangles = len(indices) // 3
    if (
        len(indices) != manifest["road_indices"]
        or triangles != manifest["road_primitives"]
        or triangles >= 2**24
    ):
        raise ValueError("road identity range is invalid")
    vertices = np.empty((len(indices), 4), dtype="<u4")
    vertices[:, :3] = snapshot.vertices[indices].view("<u4")
    vertices[:, 3] = np.repeat(np.arange(1, triangles + 1, dtype="<u4"), 3)
    vertices.tofile(output / "road-expanded.bin")
    np.arange(len(indices), dtype="<u4").tofile(output / "road-sequential.bin")
    models[0].astype("<f4").T.copy().tofile(output / "road-model.bin")
    np.asarray(manifest["raster_view"], dtype="<f4").tofile(output / "road-view.bin")
    np.array([len(indices), 1, 0, 0, 0], dtype="<u4").tofile(output / "road-args.bin")
    (output / "frame-depth.bin").write_bytes(depth)
    if digest(output / "frame-depth.bin") != manifest["depth_sha256"]:
        raise ValueError("unpacked scene depth differs")
    return manifest, depth


def results(output, triangles):
    ids, depths, rows = [], [], []
    scene = np.fromfile(output / "frame-depth.bin", dtype="<f4")
    for mode in range(2):
        identity = np.fromfile(output / f"road-id-{mode}.bin", dtype="<f4")
        depth = np.fromfile(output / f"road-depth-{mode}.bin", dtype="<f4")
        if (
            identity.shape != scene.shape
            or depth.shape != scene.shape
            or not np.isfinite(identity).all()
            or not np.isfinite(depth).all()
            or (identity < 0).any()
            or (identity > triangles).any()
            or not np.equal(identity, np.floor(identity)).all()
        ):
            raise ValueError("invalid native road readback")
        mask = identity > 0
        rows.append(
            {
                "mode": mode,
                "pixels": int(mask.sum()),
                "primitives": int(np.unique(identity[mask]).size),
                "exact_scene_depth_pixels": int(
                    np.count_nonzero(mask & (depth == scene))
                ),
            }
        )
        ids.append(identity)
        depths.append(depth)
    return ids, depths, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    checkout, output = args.checkout.resolve(), args.output.resolve()
    if output.exists() or not any(
        output.is_relative_to(p.resolve())
        for p in [Path(os.environ.get("TMPDIR", "/tmp")), Path("/tmp")]
    ):
        parser.error("use a fresh system-temp output directory")
    if output.is_relative_to(ROOT) or output.is_relative_to(checkout):
        parser.error("outputs must remain outside checkouts")
    output.mkdir(parents=True)
    os.environ.setdefault("MTL_DEBUG_LAYER", "1")
    record = {
        "scope": "fixed depth coverage diagnostic, not cost or streaming admission",
        "status": "running",
        "checkout": str(checkout),
        "steps": [],
    }
    receipt = output / "receipt.json"
    status = 1
    try:
        record["archive_sha256"] = digest(checkout / "build/liboutshine.a")
        record["compile_database_sha256"] = digest(checkout / "compile_commands.json")
        record["source_sha256"] = digest(Path(__file__).with_suffix(".cpp"))
        manifest, original_depth = prepare(output)
        record["fixture"] = manifest
        binary = build(
            checkout, output, record["steps"], Path(__file__).with_suffix(".cpp")
        )
        compiler = checkout / "build/deps/install/bin/glslangValidator"
        if not compiler.is_file():
            compiler = shutil.which("glslangValidator")
        if not compiler:
            raise ValueError("build/install glslangValidator first")
        for name, stage in [("raster", "vert"), ("road-id", "frag")]:
            source = Path(__file__).with_suffix("") / f"{name}.{stage}"
            execute(
                [
                    str(compiler),
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    str(source),
                    "-o",
                    str(output / f"{name}.spv"),
                ],
                checkout,
                output,
                f"shader-{name}",
                record["steps"],
            )
        record["runs"] = []
        baseline = None
        for case in ["scene", "empty", "blocked"]:
            depth = (
                original_depth
                if case == "scene"
                else np.full(
                    1280 * 720, 0 if case == "empty" else 1, dtype="<f4"
                ).tobytes()
            )
            (output / "frame-depth.bin").write_bytes(depth)
            execute([str(binary), str(output)], checkout, output, case, record["steps"])
            devices = re.findall(
                r"^DEVICE (.+)$", (output / f"{case}.log").read_text(), re.MULTILINE
            )
            if devices != [manifest["device"]]:
                raise ValueError("native device differs from this depth capture")
            ids, depths, rows = results(output, manifest["road_primitives"])
            record["runs"].append({"case": case, "device": devices[0], "results": rows})
            if case == "scene":
                if (
                    rows[1]["pixels"] == 0
                    or rows[1]["exact_scene_depth_pixels"] != rows[1]["pixels"]
                ):
                    raise ValueError(
                        "road replay differs from original full-scene depth"
                    )
                baseline = ids[0].copy()
            elif case == "empty" and not all(np.array_equal(x, baseline) for x in ids):
                raise ValueError("empty depth discards or changes road primitives")
            elif case == "blocked" and (ids[1].any()):
                raise ValueError("blocked depth accepts road primitives")
            np.savez_compressed(
                output / f"{case}-readbacks.npz", ids=ids, depths=depths
            )
        (output / "frame-depth.bin").write_bytes(original_depth)
        for mode in range(2):
            for family in ["road-id", "road-depth"]:
                (output / f"{family}-{mode}.bin").unlink()
        for name in ["road-expanded.bin", "road-sequential.bin"]:
            (output / name).unlink()
        shutil.rmtree(output / "geometry")
        status = 0
    except (OSError, ValueError, RuntimeError, StopIteration) as error:
        record["error"] = str(error)
        print(error, file=sys.stderr)
    finally:
        record.update(status="terminal", exit_code=status)
        receipt.write_text(json.dumps(record, indent=2) + "\n")
    if status == 0:
        print("scene/empty/blocked controls passed; " + str(receipt))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
