"""Compare bounded private terrain shader alternatives in an unchanged world."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from ground_class_distance import candidate_source
from ground_height_gather import lattice_source, normal_source
from native_tree_detail_world import checked

ROOT = Path(__file__).resolve().parents[3]

def runtime(native, output, alternative=None):
    output.mkdir()
    (output / "src").symlink_to(native / "src")
    (output / "build").mkdir()
    (output / "build/compute-shader-contracts.json").symlink_to(
        native / "build/compute-shader-contracts.json"
    )
    shaders = output / "build/shaders"
    if alternative is None:
        shaders.symlink_to(native / "build/shaders")
        return []
    shutil.copytree(native / "build/shaders", shaders)
    source = native / "src/render/shaders"
    (output / "groundLit.glsl").write_bytes((source / "groundLit.glsl").read_bytes())
    transforms = ({"groundClass.glsl": candidate_source} if alternative == "distance" else
                  {"groundLatticeCore.glsl": lattice_source, "groundFineNormal.glsl": normal_source})
    for name, transform in transforms.items():
        (output / name).write_text(transform((source / name).read_text()))
    if alternative == "gather":
        (output / "groundLatticeVertex.glsl").write_bytes((source / "groundLatticeVertex.glsl").read_bytes())
    commands = []
    names = ("SUBJECT_WRITES_VELOCITY", "SUBJECT_NORMAL_LOCATION",
             "SUBJECT_IDENTITY_LOCATION", "SUBJECT_BASE_LOCATION", "SUBJECT_METALROUGH_LOCATION")
    artifacts = sorted(shaders.glob("ground*Lit-*.frag.spv"))
    if alternative == "gather":
        artifacts += sorted(shaders.glob("ground*Lattice-*.vert.spv"))
    for artifact in artifacts:
        vertex = artifact.name.endswith(".vert.spv")
        suffix = ".vert.spv" if vertex else ".frag.spv"
        layout = artifact.name.split("-", 1)[1].removesuffix(suffix).ljust(5, "0")
        command = [str(native / "build/deps/install/bin/glslangValidator"), "-V",
                   "--target-env", "vulkan1.0", "-S", "vert" if vertex else "frag", "-Isrc",
                   "-Isrc/render/shaders", "-Isrc/content/shade", "-Ibuild/shaders"]
        if artifact.name.startswith("groundPatch"):
            command += ["-DGROUND_PATCHES=1"]
        defines = list(zip(names, layout, strict=True))[:1 if vertex else 5]
        command += [f"-D{k}={v}" for k, v in defines]
        command += [str(output / ("groundLatticeVertex.glsl" if vertex else "groundLit.glsl")),
                    "-o", str(artifact)]
        checked(["nice", "-n", "10", *command], native, output / f"{artifact.name}.log")
        commands.append(command)
    if not commands:
        raise ValueError("no ground fragment artifacts found")
    return commands


def capture(binary, directory, output, place, label):
    command = [str(binary), "shots", "--offline", "--rows", "--scenario-overrides",
               json.dumps({"render": {"fps": 30, "widthPx": 1280, "heightPx": 720}}), place]
    log = output / f"{label}.log"
    checked(command, directory, log)
    lines = log.read_text().splitlines()
    rows = [line.split("\t") for line in lines if line.startswith("ROW\t")]
    gpu = [line.split("\t") for line in lines if line.startswith("GPUROW\t")]
    if len(rows) != 1 or len(gpu) != 1 or (rows[0][3], rows[0][7], rows[0][18]) != ("1", "30", "360"):
        raise ValueError("world/frame sequence incomplete")
    if gpu[0][2:4] != ["30", "30"] or float(gpu[0][6]) != 0:
        raise ValueError("incomplete GPU timing window")
    png = directory / f"build/shots/places/{place}-{rows[0][2]}.png"
    data = png.read_bytes()
    (output / f"{label}.png").write_bytes(data)
    return {"label": label, "command": command, "png_sha256": hashlib.sha256(data).hexdigest(),
            "gpu_mean_ms": float(gpu[0][4]) / 30, "gpu_max_ms": float(gpu[0][5]),
            "row": rows[0], "gpu_row": gpu[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--place", default="Koerbersee")
    parser.add_argument("--alternative", choices=("distance", "gather"), default="distance")
    args = parser.parse_args()
    native, output = args.native_root.resolve(), args.output.resolve()
    if output.is_relative_to(native) or output.is_relative_to(ROOT) or not any(
        output.is_relative_to(p) for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    runtime(native, output / "before")
    commands = runtime(native, output / "after", args.alternative)
    binary = native / "build/outshine-client"
    report = {"native_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=native, text=True).strip(),
              "alternative": args.alternative,
              "client_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
              "compile_commands": commands, "captures": [], "accepted": False}
    for label in ("A1", "B1", "B2", "A2"):
        directory = output / ("before" if label.startswith("A") else "after")
        result = capture(binary, directory, output, args.place, label)
        report["captures"].append(result)
        (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
        print(label, result["png_sha256"][:8], result["gpu_mean_ms"], result["gpu_max_ms"], flush=True)
        if result["png_sha256"] != report["captures"][0]["png_sha256"]:
            raise ValueError("image changed; candidate is not admitted")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
