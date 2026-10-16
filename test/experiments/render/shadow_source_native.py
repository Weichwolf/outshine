"""Native production light kernel and correctly paired source-ownership inputs."""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "weather"))
from cloud_sun_reuse_native import ROOT, build_consumer, digest
from shadow_source_cases import CASES, fixture
from shadow_source_reference import HEIGHT, WIDTH, reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("output", "api-root", "archive", "archive-receipt"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--baseline", required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private captures belong outside the checkout")
    args.output.mkdir(parents=True, exist_ok=True)
    record = {"commands": []}

    def run(command, folder, name, cwd=ROOT):
        command = list(map(str, command))
        started = time.monotonic()
        r = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, check=False
        )
        (folder / f"{name}.stdout").write_text(r.stdout)
        (folder / f"{name}.stderr").write_text(r.stderr)
        record["commands"].append(
            {
                "argv": command,
                "cwd": str(cwd),
                "exit": r.returncode,
                "process_io_wall_seconds": time.monotonic() - started,
            }
        )
        (args.output / "receipt.json").write_text(json.dumps(record, indent=2))
        r.check_returncode()
        return r.stdout

    record["commit"] = run(["git", "rev-parse", "HEAD"], args.output, "commit").strip()
    record["dirty"] = run(
        ["git", "status", "--porcelain"], args.output, "status"
    ).strip()
    baseline = run(
        ["git", "rev-parse", "--verify", args.baseline], args.output, "baseline"
    ).strip()
    record["baseline"] = baseline
    shader_dir = ROOT / "src/render/shaders"
    common_names = (
        "lighting.glsl",
        "subjectLights.glsl",
        "material.glsl",
        "shadowFilter.glsl",
        "environmentSpecular.glsl",
        "environmentSampling.glsl",
        "metalRoughBrdf.glsl",
        "microfacetEnergy.glsl",
        "sheenLobe.glsl",
        "thinSheet.glsl",
        "iridescenceLobe.glsl",
    )
    for name in common_names:
        old = run(
            ["git", "show", baseline + ":src/render/shaders/" + name],
            args.output,
            "common-" + name,
        )
        if old != (shader_dir / name).read_text():
            raise RuntimeError(
                "the baseline must have identical common material/shadow equations"
            )
    run(
        ["nice", "-n", "10", "make", "build/shader-tables"],
        args.output,
        "table-builder",
    )
    (args.output / "brdfTables.glsl").write_text(
        run(["nice", "-n", "10", ROOT / "build/shader-tables"], args.output, "tables")
    )
    old = run(
        ["git", "show", baseline + ":src/render/shaders/subjectLighting.glsl"],
        args.output,
        "old-kernel",
    )
    (args.output / "before.glsl").write_text(old)
    wrapper = (ROOT / "test/experiments/render/shadow_source_probe.frag").read_text()
    (args.output / "before.frag").write_text(
        wrapper.replace('"subjectLighting.glsl"', '"before.glsl"')
    )
    glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
    for name, source in (
        ("before", args.output / "before.frag"),
        ("selected", ROOT / "test/experiments/render/shadow_source_probe.frag"),
        ("vertex", shader_dir / "fullscreenNdc.vert"),
    ):
        run(
            [
                "nice",
                "-n",
                "10",
                *glslang,
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                "-I" + str(args.output),
                source,
                "-o",
                args.output / f"{name}.spv",
            ],
            args.output,
            "compile-" + name,
        )
    cross = shlex.split(os.environ.get("SPIRV_CROSS", "spirv-cross"))
    reflection = json.loads(
        run(
            [*cross, args.output / "selected.spv", "--reflect"],
            args.output,
            "reflection",
        )
    )
    block = next(b for b in reflection["ubos"] if b["name"] == "Material")
    offsets = {
        m["name"]: m["offset"] // 4
        for m in reflection["types"][block["type"]]["members"]
    }
    api = args.api_root.resolve()
    binary, record["consumer"] = build_consumer(
        args,
        run,
        ROOT / "test/experiments/render/shadow_source_native.cpp",
        [
            api / "test/harness/render",
            api / "src/render/device/native",
            api / "test/harness/shared",
        ],
    )
    analyses = []
    for material in ("opaque", "thin"):
        for name, count, index, kind, atlas in CASES:
            tag = material + "-" + name
            before, after = fixture(offsets, material, count, index, kind, atlas)
            expected = reference(after)
            arrays, timings = {}, {}
            for mode, data in (("before", before), ("selected", after)):
                source = args.output / f"{tag}-{mode}.parameters"
                source.write_bytes(data)
                target = args.output / f"{tag}-{mode}.rgba32"
                stdout = run(
                    [
                        binary,
                        args.output / "vertex.spv",
                        args.output / f"{mode}.spv",
                        source,
                        target,
                    ],
                    args.output,
                    tag + "-" + mode,
                )
                entries = re.findall(
                    r"gpu_submission phase=(\S+) iteration=(\d+) nanoseconds=(\S+)",
                    stdout,
                )
                if [(p, int(i)) for p, i, _ in entries] != [
                    ("prepare", 0),
                    *(("draw-readback", i) for i in range(4)),
                ]:
                    raise RuntimeError("missing actual preparation/draw GPU timestamps")
                values = [float(v) / 1e6 for _, _, v in entries]
                if not np.isfinite(values).all() or min(values) < 0:
                    raise RuntimeError("invalid GPU timestamp interval")
                timings[mode] = {
                    "prepare_ms": values[0],
                    "first_draw_readback_ms": values[1],
                    "repeat_draw_readback_ms": values[2:],
                }
                arrays[mode] = np.fromfile(target, "<f4").reshape(HEIGHT, WIDTH, 4)
                if (
                    not np.isfinite(arrays[mode]).all()
                    or np.any(arrays[mode] < 0)
                    or np.any(arrays[mode][..., 3] != 1)
                ):
                    raise RuntimeError(
                        "the actual light kernel violated finite RGB/alpha"
                    )
            error = float(np.max(abs(arrays["selected"][..., :3] - expected)))
            previous = float(np.max(abs(arrays["before"][..., :3] - expected)))
            unchanged = arrays["before"].tobytes() == arrays["selected"].tobytes()
            if error > 2e-6 or (
                name in ("single", "none", "point", "spot") and not unchanged
            ):
                raise RuntimeError(
                    "explicit ownership changed independent light response or valid old controls"
                )
            if name in ("first", "last", "middle16", "last16") and previous < 0.1:
                raise RuntimeError(
                    "the original cross-light occlusion control must fail"
                )
            analyses.append(
                {
                    "case": tag,
                    "RGB_abs": error,
                    "previous_RGB_abs": previous,
                    "unchanged": unchanged,
                    "source_index": index,
                    "light_count": count,
                    "submission_gpu": timings,
                }
            )
            if material == "opaque" and name == "last":
                target = args.output / "mixed-old-producer.rgba32"
                source = args.output / f"{tag}-before.parameters"
                run(
                    [
                        binary,
                        args.output / "vertex.spv",
                        args.output / "selected.spv",
                        source,
                        target,
                    ],
                    args.output,
                    "mixed-old-producer",
                )
                mixed = np.fromfile(target, "<f4").reshape(HEIGHT, WIDTH, 4)
                mismatch = float(np.max(abs(mixed[..., :3] - expected)))
                if mismatch < 0.1:
                    raise RuntimeError(
                        "the old boolean producer must fail with the new source-index consumer"
                    )
                record["mixed_old_producer_RGB_abs"] = mismatch
    report = {
        "scope": "actual native shadeRow, controlled tilted receiver/constant shadow map and ideal diffuse opaque/thin materials; no world pipeline or GPU/frame budget claim",
        "RGB_absolute_limit": 2e-6,
        "cases": analyses,
    }
    (args.output / "analysis.json").write_text(json.dumps(report, indent=2))
    record["analysis"] = report
    record["source_hashes"] = {
        str(p.relative_to(ROOT)): digest(p)
        for p in [
            shader_dir / "subjectLighting.glsl",
            *(ROOT / "test/experiments/render").glob("shadow_source*"),
        ]
        if p.is_file()
    }
    record["capture_hashes"] = {
        str(p.relative_to(args.output)): digest(p)
        for p in args.output.iterdir()
        if p.suffix in (".parameters", ".rgba32", ".spv")
    }
    (args.output / "receipt.json").write_text(json.dumps(record, indent=2))
    print(
        "Explicit native shadow-source controls passed:", args.output / "receipt.json"
    )


if __name__ == "__main__":
    main()
