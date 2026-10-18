"""Compare existing tree detail in one actual offline world; all outputs stay in temp."""

import argparse
import hashlib
import json
import math
import re
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
METHODS = ("mesh", "flat", "depth", "points", "normal")


def checked(command, cwd, log, timeout=120):
    with log.open("w") as stream:
        result = subprocess.run(
            command,
            cwd=cwd,
            stdout=stream,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
    if result.returncode:
        raise RuntimeError(f"exit{result.returncode}: {log}")


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"native source boundary changed: {old[:70]}")
    return source.replace(old, new, 1)


def instrument(native, output, dump):
    source = (native / "src/engine/streaming/VegetationStreaming.cpp").read_text()
    include = f"#include {json.dumps(str(HERE / 'NativeTreeDetailPreview.h'))}"
    source = replace_once(
        source,
        '#include "VegetationStreaming.h"',
        '#include "VegetationStreaming.h"\n' + include,
    )
    source = replace_once(
        source,
        "Shape_(config.Shape) {}",
        """Shape_(config.Shape) {
      NativeTreeDetailProbe::Directory = config.Cache.Directory;
      NativeTreeDetailProbe::DumpPath = DUMP_PATH;
      NativeTreeDetailProbe::SelectionPath = SELECTION_PATH;
    }""".replace("DUMP_PATH", json.dumps(str(output / "world.json"))).replace(
            "SELECTION_PATH", json.dumps(str(output / "selection.txt"))
        ),
    )
    source = replace_once(
        source,
        "VegetationStreaming::~VegetationStreaming() {",
        "VegetationStreaming::~VegetationStreaming() {\n"
        "  NativeTreeDetailProbe::Held.Clear();",
    )
    anchor = "  if (prepare) { PrepareNext(); }\n  for (auto &group : Groups_) {"
    body = (
        "  if (!prepare && Ready()) { NativeTreeDetailProbe::Dump(Groups_,eye); }"
        if dump
        else """  if (prepare && Ready() && !Groups_.empty() &&
      NativeTreeDetailProbe::Held.Renderer == nullptr && publication == ResourcePublication::Allowed &&
      !NativeTreeDetailProbe::Prepare(Groups_,*Renderer_,error)) { return false; }"""
    )
    source = replace_once(
        source,
        anchor,
        "  if (prepare) { PrepareNext(); }\n"
        + body
        + "\n  for (auto &group : Groups_) {",
    )
    source = replace_once(
        source,
        "if (!group.Pieces->Update(group.Models, eye, error)) {",
        """
    const auto &models = NativeTreeDetailProbe::Held.Renderer != nullptr
      ? NativeTreeDetailProbe::Held.Far[static_cast<size_t>(&group-Groups_.data())] : group.Models;
    if (!group.Pieces->Update(models, eye, error)) {""",
    )
    source = replace_once(
        source,
        "    SelectedInstances_ += group.Models.size();\n  }\n  return true;",
        "    SelectedInstances_ += group.Models.size();\n  }\n"
        "  return NativeTreeDetailProbe::Update(eye,error);",
    )
    return source


def compile_unit(database, native, original, source, target):
    entry = next(row for row in database if row["file"].endswith("/" + original))
    command = shlex.split(entry["command"])
    command[command.index(entry["file"])] = str(source)
    command[command.index("-o") + 1] = str(target)
    return command


def libraries(native):
    runner = (
        (native / "test/run.sh")
        .read_text()
        .split("LayerLink() {", 1)[1]
        .split("\n}", 1)[0]
    )
    line = next(
        row
        for row in runner.splitlines()
        if row.lstrip().startswith("profile/engine | harness/wpt/css")
    )
    packages = re.search(r"\$\(pkg-config --libs ([^)]+)\)", line).group(1).split()
    package_flags = subprocess.check_output(
        ["pkg-config", "--libs", *packages], text=True
    )
    backend_flags = subprocess.check_output(
        ["sh", "test/run.sh", "--native-link-flags"], cwd=native, text=True
    )
    return [*shlex.split(package_flags), "-lz", "-lcurl", *shlex.split(backend_flags)]


def build(native, output, database, extra, label, dump=False):
    source = output / f"{label}.cpp"
    source.write_text(instrument(native, output, dump))
    obj = output / f"{label}.o"
    command = compile_unit(database, native, "VegetationStreaming.cpp", source, obj)
    checked(["nice", "-n", "10", *command], native, output / f"{label}-compile.log")
    compiler = shlex.split(database[0]["command"])[0]
    binary = output / label
    link = [
        compiler,
        str(output / "main.o"),
        str(obj),
        *map(str, extra),
        str(native / "build/liboutshine.a"),
        str(native / "build/libgenerators.a"),
        *libraries(native),
        "-o",
        str(binary),
    ]
    checked(["nice", "-n", "10", *link], native, output / f"{label}-link.log")
    return binary, [command, link]


def alternative(native, output, database, method):
    if method not in ("points", "normal"):
        return []
    name = "DepthSurfacePatches.cpp" if method == "points" else "ImpostorSurface.cpp"
    source = (native / "src/render/impostor" / name).read_text()
    if method == "points":
        source = replace_once(
            source,
            "  patch.SlopeX = sx;\n  patch.SlopeY = sy;",
            "  sx = 0;\n  sy = 0;\n  patch.SlopeX = sx;\n  patch.SlopeY = sy;",
        )
    else:
        source = (
            f"#include {json.dumps(str(HERE / 'NativeTreeDepthNormals.h'))}\n" + source
        )
        start = source.index("  return BuildDepthSurfacePatches({.Width = pixels,")
        end = source.index("\n}", start)
        block = source[start:end].replace(
            "return BuildDepthSurfacePatches", "auto patches = BuildDepthSurfacePatches"
        )
        source = (
            source[:start]
            + block
            + """
  if (!patches) { return std::unexpected(patches.error()); }
  return NativeTreeDetailProbe::NormalPatches(atlas,source,*patches);"""
            + source[end:]
        )
    path, obj = output / f"{method}-surface.cpp", output / f"{method}-surface.o"
    path.write_text(source)
    command = compile_unit(database, native, name, path, obj)
    checked(
        ["nice", "-n", "10", *command], native, output / f"{method}-surface-compile.log"
    )
    (output / f"{method}-surface-command.json").write_text(
        json.dumps(command, indent=2)
    )
    return [obj]


def choose(world, bearing, pitch, count, visible):
    angle, pitch = math.radians(bearing), math.radians(pitch)
    forward = (
        math.sin(angle) * math.cos(pitch),
        math.sin(pitch),
        -math.cos(angle) * math.cos(pitch),
    )
    right = (math.cos(angle), 0.0, math.sin(angle))
    choices = []
    for group, data in enumerate(world["groups"]):
        for row, model in enumerate(data["models"]):
            delta = [model[12 + axis] - world["eye"][axis] for axis in range(3)]
            depth = sum(a * b for a, b in zip(delta, forward, strict=True))
            side = sum(a * b for a, b in zip(delta, right, strict=True))
            if visible and (depth <= 1 or abs(side / depth) >= 0.65):
                continue
            choices.append(
                (math.sqrt(sum(value * value for value in delta)), group, row)
            )
    selected = sorted(choices)[:count]
    if len(selected) != count:
        raise ValueError("world has too few candidates")
    return selected


def capture(binary, runtime, output, place, label):
    log = output / f"{label}.log"
    checked(
        [
            str(binary),
            "shots",
            "--rows",
            "--stats",
            "--offline",
            "--preload-seconds",
            "60",
            "--scenario-overrides",
            '{"render":{"fps":30,"widthPx":1280,"heightPx":720}}',
            place,
        ],
        runtime,
        log,
        timeout=180,
    )
    image = next((runtime / "build/shots/places").glob(f"{place}-*.png"))
    saved = output / f"{label}.png"
    shutil.copy2(image, saved)
    rows = [
        line
        for line in log.read_text().splitlines()
        if line.startswith(("DETAIL ", "ROW\t", "GPUROW\t"))
    ]
    row = next(line.split("\t") for line in rows if line.startswith("ROW\t"))
    if (
        row[3] != "1"
        or int(row[7]) != 30
        or row[11] != "0"
        or row[13] != "1"
        or float(row[18]) != 360
    ):
        raise RuntimeError(f"incomplete source world/capture: {log}")
    if image.stem != f"{place}-{row[2]}":
        raise RuntimeError(f"capture did not replace its image: {log}")
    return {
        "label": label,
        "png_sha256": hashlib.sha256(saved.read_bytes()).hexdigest(),
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--place", required=True)
    parser.add_argument("--bearing-deg", required=True, type=float)
    parser.add_argument("--pitch-deg", required=True, type=float)
    parser.add_argument("--count", type=int, default=4, choices=range(1, 17))
    parser.add_argument("--rank", type=int, default=2, choices=range(4))
    parser.add_argument("--selection", choices=("near", "front"), default="front")
    parser.add_argument("--methods", nargs="+", choices=METHODS, default=list(METHODS))
    args = parser.parse_args()
    native, output = args.native_root.resolve(), args.output.resolve()
    if not any(
        output.is_relative_to(root)
        for root in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("output must be under the system temp directory")
    if output.is_relative_to(native) or output.is_relative_to(ROOT):
        parser.error("output must be outside the source checkouts")
    output.mkdir(parents=True, exist_ok=False)
    runtime = output / "runtime"
    (runtime / "build").mkdir(parents=True)
    (runtime / "src").symlink_to(native / "src", target_is_directory=True)
    for name in ("shaders", "compute-shaders.json", "shader-artifacts.txt"):
        (runtime / "build" / name).symlink_to(native / "build" / name)
    database = json.loads((native / "compile_commands.json").read_text())
    commands = [
        compile_unit(
            database,
            native,
            "Main.cpp",
            native / "src/client/Main.cpp",
            output / "main.o",
        )
    ]
    checked(["nice", "-n", "10", *commands[0]], native, output / "main-compile.log")
    binary, built = build(native, output, database, [], "dump", dump=True)
    commands.extend(built)
    results = [capture(binary, runtime, output, args.place, "source-dump")]
    world = json.loads((output / "world.json").read_text())
    selected = choose(
        world, args.bearing_deg, args.pitch_deg, args.count, args.selection == "front"
    )
    for method in args.methods:
        mode = 0 if method == "mesh" else 1 if method == "flat" else 2
        lines = [f"{len(selected)} {mode} {args.rank}"]
        for _, group, row in selected:
            lines.append(
                f"{group} {row} "
                + " ".join(
                    format(value, ".17g")
                    for value in world["groups"][group]["models"][row]
                )
            )
        (output / "selection.txt").write_text("\n".join(lines) + "\n")
        extra = alternative(native, output, database, method)
        binary, built = build(native, output, database, extra, method)
        commands.extend(built)
        results.append(capture(binary, runtime, output, args.place, method))
        if method == "flat" and results[-1]["png_sha256"] != results[0]["png_sha256"]:
            raise AssertionError("rebuilt flat control differs from original world")
    metadata = {
        "native_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=native, text=True
        ).strip(),
        "source_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "world_sha256": hashlib.sha256(
            (output / "world.json").read_bytes()
        ).hexdigest(),
        "selected": selected,
        "selection": args.selection,
        "parameters": {
            key: str(value) if isinstance(value, Path) else value
            for key, value in vars(args).items()
        },
        "experiment_sha256": {
            name: hashlib.sha256((HERE / name).read_bytes()).hexdigest()
            for name in (
                "native_tree_detail_world.py",
                "NativeTreeDetailPreview.h",
                "NativeTreeDepthNormals.h",
            )
        },
        "results": results,
        "scope": "one world; diagnostic geometry, not cached/accepted engine products",
        "commands": commands,
    }
    (output / "receipt.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"{len(results)} rendered controls: {output / 'receipt.json'}")


if __name__ == "__main__":
    main()
