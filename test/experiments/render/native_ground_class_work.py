"""Export bound native classification/palette buffers and count cell-list work."""

import argparse
import hashlib
import json
import shlex
import subprocess
import tempfile
from pathlib import Path

from ground_class_work import analyse

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def run(command, directory, log):
    with log.open("w") as stream:
        subprocess.run(
            command, cwd=directory, stdout=stream, stderr=subprocess.STDOUT, check=True
        )


def build(native, output):
    database = json.loads((native / "build/compile_commands.json").read_text())
    readback = native / "src/render/scene/SceneReadback.cpp"
    source = readback.read_text()
    needle = "ReadState SceneRenderer::ReadPixels(std::vector<uint8_t> &rgba) {"
    if source.count(needle) != 1:
        raise ValueError("native readback boundary changed")
    source = source.replace(
        needle,
        needle + "\n  if (!GroundClassInventory::Capture(Device_.get(), "
        "ActiveContent().Scene().GroundMaterials().Classes(), "
        "ActiveContent().Scene().Resources().GroundClasses(), "
        + json.dumps(str(output / "classes.u32"))
        + ")) { return ReadState::Failed; }\n"
        "  if (!GroundClassInventory::Capture(Device_.get(), "
        "ActiveContent().Scene().GroundMaterials().Palette(), "
        "ActiveContent().Scene().Resources().GroundPalette(), "
        + json.dumps(str(output / "palette.f32"))
        + ")) { return ReadState::Failed; }",
    )
    proxy = output / "readback.cpp"
    proxy.write_text(
        f"#include {json.dumps(str(HERE / 'GroundClassInventory.h'))}\n" + source
    )
    commands, contexts = [], []
    for original, actual, target in (
        (readback, proxy, output / "readback.o"),
        (native / "src/app/Main.cpp", native / "src/app/Main.cpp", output / "main.o"),
    ):
        row = next(r for r in database if Path(r["file"]).resolve() == original)
        command = (row.get("arguments") or shlex.split(row["command"])).copy()
        command[command.index(row["file"])] = str(actual)
        command[command.index("-o") + 1] = str(target)
        run(
            ["nice", "-n", "10", *command],
            row["directory"],
            output / (target.stem + ".log"),
        )
        commands.append(command)
        contexts.append(
            {"directory": row["directory"], "file": str(actual), "arguments": command}
        )
    flags = shlex.split(
        subprocess.check_output(
            ["sh", "test/run.sh", "--native-link-flags"], cwd=native, text=True
        )
    )
    command = [
        commands[0][0],
        str(output / "main.o"),
        str(output / "readback.o"),
        str(native / "build/liboutshine.a"),
        *flags,
        "-o",
        str(output / "client"),
    ]
    run(command, native, output / "link.log")
    (output / "compile_commands.json").write_text(json.dumps(contexts, indent=2) + "\n")
    return [*commands, command]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", required=True, type=Path)
    parser.add_argument("--command-json", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    native, output = args.native_root.resolve(), args.output.resolve()
    if (
        output.is_relative_to(ROOT)
        or output.is_relative_to(native)
        or not any(
            output.is_relative_to(p)
            for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
        )
    ):
        parser.error("private output belongs outside the checkout in system temp")
    command = json.loads(args.command_json.read_text())
    if (
        not isinstance(command, list)
        or len(command) < 2
        or command[1] != "benchmark"
        or "--offline" not in command
    ):
        parser.error("use a generic offline benchmark argv array")
    for flag in ("--image", "--output"):
        if flag not in command or command.index(flag) + 1 >= len(command):
            parser.error(f"benchmark requires {flag}")
    output.mkdir(parents=True, exist_ok=False)
    (output / "assets").symlink_to(
        (native / "build/assets").resolve(), target_is_directory=True
    )
    commands = build(native, output)
    command[0] = str(output / "client")
    for flag, filename in (("--image", "image.png"), ("--output", "benchmark.json")):
        command[command.index(flag) + 1] = str(output / filename)
    (output / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    run(command, native, output / "capture.log")
    benchmark = json.loads((output / "benchmark.json").read_text())
    if (
        not benchmark["measurement_complete"]
        or not benchmark["world_complete"]
        or benchmark["unrefined_frames"]
    ):
        raise ValueError("native benchmark did not submit a complete world/window")
    report = analyse(output / "classes.u32")
    palette = (output / "palette.f32").read_bytes()
    report.update(
        palette={"bytes": len(palette), "sha256": hashlib.sha256(palette).hexdigest()},
        gpu_bytes_equal=True,
        commands=[*commands, command],
        native_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=native, text=True
        ).strip(),
        native_status=subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=native, text=True
        ).strip(),
        source_sha256={
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                output / "readback.cpp",
                HERE / "GroundClassInventory.h",
                native / "src/render/shaders/groundClass.glsl",
                native / "build/liboutshine.a",
            )
        },
        client_sha256=hashlib.sha256((output / "client").read_bytes()).hexdigest(),
    )
    (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
