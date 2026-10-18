"""Export the classifier actually held at the final native world screenshot."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from ground_class_work import analyse
from native_tree_detail_world import checked, compile_unit, libraries, replace_once

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def build(native, output):
    database = json.loads((native / "compile_commands.json").read_text())
    source = (native / "src/render/SceneRenderer.cpp").read_text()
    header = HERE / "GroundClassInventory.h"
    source = f"#include {json.dumps(str(header))}\n" + source
    needle = "ReadState SceneRenderer::ReadPixels(std::vector<uint8_t> &rgba) {"
    injection = (
        needle
        + "\n  if (!GroundClassInventory::Capture(Device_.get(), ActiveState().Content.Ground.Classes(), ActiveState().Content.Resources.GroundClasses(),"
        + json.dumps(str(output / "classes.u32"))
        + ")) { return ReadState::Failed; }\n"
    )
    source = replace_once(source, needle, injection)
    target = output / "renderer.cpp"
    target.write_text(source)
    commands = []
    for name, actual, obj in (
        ("SceneRenderer.cpp", target, output / "renderer.o"),
        ("Main.cpp", native / "src/client/Main.cpp", output / "main.o"),
    ):
        command = compile_unit(database, native, name, actual, obj)
        checked(["nice", "-n", "10", *command], native, output / f"{name}.log")
        commands.append(command)
    command = [
        commands[0][0],
        str(output / "main.o"),
        str(output / "renderer.o"),
        str(native / "build/liboutshine.a"),
        str(native / "build/libgenerators.a"),
        *libraries(native),
        "-o",
        str(output / "client"),
    ]
    checked(["nice", "-n", "10", *command], native, output / "link.log")
    return [*commands, command]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--place", required=True)
    args = parser.parse_args()
    native, output = args.native_root.resolve(), args.output.resolve()
    if output.is_relative_to(ROOT) or not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private output belongs outside the checkout in system temp")
    output.mkdir(parents=True, exist_ok=False)
    commands = build(native, output)
    (output / "src").symlink_to(native / "src")
    (output / "build").mkdir()
    for name in ("shaders", "compute-shader-contracts.json"):
        (output / "build" / name).symlink_to(native / "build" / name)
    command = [
        str(output / "client"),
        "shots",
        "--offline",
        "--rows",
        "--scenario-overrides",
        json.dumps({"render": {"fps": 30, "widthPx": 1280, "heightPx": 720}}),
        args.place,
    ]
    checked(command, output, output / "capture.log")
    lines = (output / "capture.log").read_text().splitlines()
    captures = [line.split("\t") for line in lines if line.startswith("ROW\t")]
    if len(captures) != 1 or (captures[0][3], captures[0][7], captures[0][18]) != (
        "1", "30", "360"
    ):
        raise ValueError("incomplete world or unexpected frame/turn sequence")
    report = analyse(output / "classes.u32")
    report.update(
        place=args.place,
        gpu_bytes_equal=True,
        native_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=native, text=True
        ).strip(),
        native_status=subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=native, text=True
        ).strip(),
        source_sha256={
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                output / "renderer.cpp",
                HERE / "GroundClassInventory.h",
                native / "src/render/shaders/groundClass.glsl",
                native / "build/liboutshine.a",
                native / "build/libgenerators.a",
            )
        },
        commands=[*commands, command],
        client_sha256=hashlib.sha256((output / "client").read_bytes()).hexdigest(),
        rows=[
            line
            for line in lines
            if line.startswith(("ROW\t", "GPUROW\t"))
        ],
    )
    (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
