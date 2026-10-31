"""Measure the native instanced-box opponent for shared urban height fields."""

import argparse
import hashlib
import json
import math
import os
import platform
import random
import sys
from pathlib import Path

import subprocess
import tempfile
import time

from native_tree_detail_world import checked, compile_unit, libraries

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "transport"))
from native_package_storage import open_packages

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def packages(database):
    with open_packages(f"file:{database}?mode=ro", uri=True) as connection:
        return [(key, len(blob), hashlib.sha256(blob).hexdigest(), crc)
                for key, blob, crc in connection.execute("SELECT key,bytes,crc FROM native_packages")]


def build(native, output):
    database = json.loads((native / "compile_commands.json").read_text())
    bounds = HERE / "urban_fields/NativeUrbanFieldBounds.cpp"
    bounds_command = compile_unit(database, native, "Engine.cpp", bounds, output / "bounds.o")
    bounds_command.insert(1, f"-I{native / 'test/harness/shared'}")
    checked(["nice", "-n", "10", *bounds_command], native, output / "bounds-compile.log", 180)
    runtime_flags = [flag for flag in bounds_command[1:] if flag.startswith("-stdlib=")]
    checked([bounds_command[0], *runtime_flags, str(output / "bounds.o"),
             "-o", str(output / "bounds")], native, output / "bounds-link.log", 180)
    checked([str(output / "bounds")], native, output / "bounds-run.log", 30)
    source = HERE / "urban_fields/NativeUrbanFieldControl.cpp"
    obj, binary = output / "control.o", output / "control"
    compile_command = compile_unit(database, native, "Engine.cpp", source, obj)
    compile_command.insert(1, f"-I{native / 'test/harness/shared'}")
    checked(["nice", "-n", "10", *compile_command], native, output / "compile.log", 180)
    runtime_flags = [flag for flag in compile_command[1:] if flag.startswith("-stdlib=")]
    command = [compile_command[0], *runtime_flags, str(obj),
               str(native / "build/liboutshine.a"), *libraries(native), "-o", str(binary)]
    checked(["nice", "-n", "10", *command], native, output / "link.log", 180)
    return binary, {"compile": compile_command, "link": command,
                    "flags_from": "Engine.cpp", "source_sha256": sha(source),
                    "header_sha256": sha(source.with_name("NativeUrbanField.h")),
                    "bounds_sha256": sha(bounds), "bounds_checked": True,
                    "library_sha256": sha(native / "build/liboutshine.a")}


def run(binary, native, directory, key, source, label, args):
    prefix = directory / label
    command = [str(binary), str(directory / "assets.sqlite"), key, str(source), str(prefix)]
    altitude = getattr(args, "altitude", None)
    if args.radius is not None or args.validation or args.sequence != "orbit" or altitude is not None:
        command.append(str(args.radius) if args.radius is not None else "auto")
    if args.validation or args.sequence != "orbit" or altitude is not None:
        command.append("validate" if args.validation else "normal")
    if args.sequence != "orbit" or altitude is not None:
        command.append(args.sequence)
    if altitude is not None:
        command.append(str(altitude))
    validation = args.validation
    environment = dict(os.environ)
    for name in ("MTL_DEBUG_LAYER", "MTL_SHADER_VALIDATION", "OUTSHINE_METAL_ENCODER_PROFILE",
                 "VK_INSTANCE_LAYERS", "VK_LOADER_LAYERS_ENABLE", "VK_LAYER_ENABLES",
                 "VK_LAYER_VALIDATE_SYNC"):
        environment.pop(name, None)
    if validation:
        environment["MTL_DEBUG_LAYER"] = "1"
        if platform.system() == "Darwin":
            environment["MTL_SHADER_VALIDATION"] = "1"
        if platform.system() == "Linux":
            environment["VK_LAYER_VALIDATE_SYNC"] = "1"
    started = time.monotonic()
    with (directory / f"{label}.log").open("w") as log:
        subprocess.run(command, cwd=native, env=environment, stdout=log,
                       stderr=subprocess.STDOUT, check=True, timeout=120)
    text = (directory / f"{label}.log").read_text()
    if any(marker in text for marker in ("VUID-", "SYNC-HAZARD", "Validation Error", "failed assertion")):
        raise RuntimeError(f"GPU validation diagnostic: {directory / f'{label}.log'}")
    device = next(row.removeprefix("DEVICE ") for row in text.splitlines() if row.startswith("DEVICE "))
    line = next(row for row in text.splitlines() if row.startswith("URBAN "))
    counters = dict(word.split("=", 1) for word in line.split()[1:])
    changed = next(row for row in text.splitlines() if row.startswith("CHANGES "))
    changes = dict(word.split("=", 1) for word in changed.split()[1:])
    samples = [row.split() for row in prefix.with_suffix(".times").read_text().splitlines()]
    submissions = [int(row[0]) for row in samples]
    assert len(submissions) == 60 and submissions == sorted(set(submissions))
    values = [float(row[1]) for row in samples]
    assert len(values) == 60 and all(math.isfinite(v) and v >= 0 for v in values)
    assert counters["okay"] == "true" and counters["raster_frames"] == "60"
    assert counters["device_creations"] == "1"
    ordered = sorted(values)
    hosts = sorted(float(row[2]) for row in samples)
    return {"command": command, "returncode": 0, "wall_seconds": time.monotonic() - started,
            "validation": validation, "gpu_device": device, "counters": counters,
            "changes": changes, "gpu_samples": len(values),
            "gpu_ms": {"p50": (ordered[29] + ordered[30]) / 2,
                       "p95": ordered[math.ceil(0.95 * len(values)) - 1],
                       "p99": ordered[math.ceil(0.99 * len(values)) - 1]},
            "host_frame_ms": {"p50": (hosts[29] + hosts[30]) / 2,
                              "p99": hosts[math.ceil(0.99 * len(hosts)) - 1]},
            "image_sha256": sha(prefix.with_suffix(".png")),
            "depth_sha256": sha(prefix.with_suffix(".depth"))}


def field(size, empty=False):
    generator = random.Random(1844)
    spacing = max(1, size // 8)
    tops = [0 if empty or x % spacing == 0 or y % spacing == 0
            else generator.randint(8, 24) for y in range(size) for x in range(size)]
    return f"{size} {size} {512 / size} 0.72\n" + " ".join(map(str, tops)) + "\n"


def verify_transitions(binary, native, directory, key, baseline, args):
    variants = {}
    for sequence in ("relit", "relight", "replaced", "replace"):
        selected = argparse.Namespace(**vars(args))
        selected.sequence = sequence
        row = run(binary, native, directory, key, "-", sequence, selected)
        expected = "1" if sequence in ("relight", "replace") else "0"
        assert row["changes"]["applied"] == expected
        expected_light_calls = "1" if sequence in ("relight", "relit") else "0"
        assert row["changes"]["light_setter_calls"] == expected_light_calls
        variants[sequence] = row
    for changed, fresh in (("relight", "relit"), ("replace", "replaced")):
        for prop in ("image_sha256", "depth_sha256"):
            assert variants[changed][prop] == variants[fresh][prop], (changed, prop)
    assert baseline["depth_sha256"] == variants["relit"]["depth_sha256"]
    if int(baseline["counters"]["instances"]) > 0:
        assert baseline["image_sha256"] != variants["relit"]["image_sha256"]
        assert baseline["depth_sha256"] != variants["replaced"]["depth_sha256"]
    else:
        assert baseline["depth_sha256"] == variants["replaced"]["depth_sha256"]
    return {"variants": variants, "matches_fresh": True,
            "prior_frame_pending_before_release":
                variants["replace"]["changes"]["pending_before_release"] == "true"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--field", type=Path)
    parser.add_argument("--radius", type=float, help="model-space orbit radius in metres")
    parser.add_argument("--altitude", type=float, help="model-space camera height in metres")
    parser.add_argument("--validation", action="store_true")
    parser.add_argument("--sequence", choices=("orbit", "relight", "relit", "replace", "replaced", "verify"),
                        default="orbit", help="functional mutation controls; timings are diagnostic")
    args = parser.parse_args()
    if args.radius is not None and (not math.isfinite(args.radius) or args.radius <= 0):
        parser.error("radius must be finite and positive")
    if args.altitude is not None and (not math.isfinite(args.altitude) or args.altitude <= 0):
        parser.error("altitude must be finite and positive")
    native, output = args.native.resolve(), args.output.resolve()
    if not any(output.is_relative_to(root.resolve()) for root in
               (Path(tempfile.gettempdir()), Path("/tmp"))):
        parser.error("output must be in the system temp directory")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    binary, build_record = build(native, output)
    selected = argparse.Namespace(**vars(args))
    if args.sequence == "verify":
        selected.sequence = "orbit"
    inputs = [("provided", args.field.read_text())] if args.field else [
        (f"grid-{size}", field(size)) for size in (32, 64, 128)] + [("empty", field(8, True))]
    result = {"commit": subprocess.check_output(["git", "rev-parse", "HEAD"],
                                               cwd=native, text=True).strip(),
              "platform": platform.platform(), "resolution": [1280, 720],
              "radius_m": args.radius, "altitude_m": args.altitude, "sequence": args.sequence,
              "build": build_record, "scope": "model-space controls; no complete-world admission",
              "cases": []}
    for name, text in inputs:
        directory = output / name
        directory.mkdir()
        source = directory / "input.field"
        source.write_text(text)
        key = sha(source)
        cold = run(binary, native, directory, key, source, "cold", selected)
        before = packages(directory / "assets.sqlite")
        retained = source.with_suffix(".retained")
        source.rename(retained)
        warm = run(binary, native, directory, key, "-", "warm", selected)
        assert before == packages(directory / "assets.sqlite")
        assert cold["image_sha256"] == warm["image_sha256"]
        assert cold["depth_sha256"] == warm["depth_sha256"]
        case = {"name": name, "source_sha256": key,
                "cold": cold, "warm": warm, "packages_unchanged": True}
        if args.sequence == "verify":
            case["transitions"] = verify_transitions(binary, native, directory, key, warm, args)
            assert before == packages(directory / "assets.sqlite")
        result["cases"].append(case)
        (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
        (directory / "warm.png").unlink()
        (directory / "warm.depth").unlink()
        print(name, json.dumps({"instances": warm["counters"]["instances"],
                                "gpu_ms": warm["gpu_ms"], "equal": True}), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
