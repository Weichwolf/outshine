"""Build a private RuntimeScene owner probe against an existing native client archive."""

import argparse
import hashlib
import json
import shlex
import subprocess
import tempfile
import time
from pathlib import Path


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def entry_arguments(database, suffix):
    matches = [entry for entry in database if entry["file"].endswith(suffix)]
    if len(matches) != 1:
        raise ValueError(f"expected one compile database entry for {suffix}")
    entry = matches[0]
    return entry, list(entry.get("arguments") or shlex.split(entry["command"]))


def instrument(source, header):
    call = "StorageProbe::Report({phase}, this, Held_.Snapshot(), DrivenGeometry_, ShapeParts_);"
    anchors = (
        ("  Held_.SetGeometry(std::move(built));", "geometry-held"),
        ("  Render::AppendGeometry(Held_.Snapshot(), ShapeParts_);", "geometry-packed"),
        ("    ShapeCooking_.reset();\n    BuildStage_ = GeometryBuildStage::Plan;", "geometry-cooked"),
        ("RuntimeScene::~RuntimeScene() {", "scene-destroy"),
    )
    for anchor, phase in anchors:
        if source.count(anchor) != 1:
            raise ValueError(f"source changed: expected exactly one {phase} anchor")
        source = source.replace(anchor, anchor + "\n  " + call.format(phase=json.dumps(phase)), 1)
    return f"#include {json.dumps(str(header))}\n" + source


def client_link(path, checkout, object_path, output):
    command = json.loads(path.read_text())
    if not isinstance(command, list) or not command or not all(isinstance(x, str) for x in command):
        raise ValueError("link-command must be a JSON array of actual client-link arguments")
    archive = checkout / "build/liboutshine.a"
    matches = [i for i, value in enumerate(command)
               if not value.startswith("-") and (checkout / value).resolve() == archive]
    if len(matches) != 1 or command.count("-o") != 1 or "-c" in command:
        raise ValueError("link-command must link build/liboutshine.a once and have one -o")
    if "-flto" in command or any(value.startswith("-flto=") for value in command):
        raise ValueError("LTO interposition is not supported by this probe")
    command.insert(matches[0], str(object_path))
    command[command.index("-o") + 1] = str(output / "outshine-client-owners")
    return command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--link-command", type=Path,
                        help="JSON argv of this checkout's regular client link; omit to compile only")
    args = parser.parse_args()
    checkout = args.checkout.resolve()
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    script_checkout = Path(__file__).resolve().parents[3]
    if (output == temporary or not output.is_relative_to(temporary)
            or output.is_relative_to(checkout) or output.is_relative_to(script_checkout)):
        parser.error("output must be a dedicated system-temp directory outside both checkouts")
    source_path = checkout / "src/engine/RuntimeScene.cpp"
    database_path = checkout / "compile_commands.json"
    header = Path(__file__).with_name("scene_storage_rows.h").resolve()
    try:
        database = json.loads(database_path.read_text())
        entry, compile_command = entry_arguments(database, "src/engine/RuntimeScene.cpp")
        if Path(entry["directory"]).resolve() != checkout:
            raise ValueError("compile database belongs to a different checkout")
        if compile_command.count("-o") != 1 or "-c" not in compile_command:
            raise ValueError("RuntimeScene compile entry must have -c and one -o")
        private_source = instrument(source_path.read_text(), header)
        object_path = output / "RuntimeScene.owners.o"
        compile_command[compile_command.index(entry["file"])] = str(output / "RuntimeScene.owners.cpp")
        compile_command[compile_command.index("-o") + 1] = str(object_path)
        link_command = client_link(args.link_command, checkout, object_path, output) \
            if args.link_command else None
    except (OSError, ValueError) as error:
        parser.error(str(error))
    output.mkdir(parents=True, exist_ok=True)
    (output / "RuntimeScene.owners.cpp").write_text(private_source)
    archive = checkout / "build/liboutshine.a"
    paths = (source_path, database_path, header, Path(__file__).resolve())
    if link_command:
        paths += (archive, args.link_command.resolve(), checkout / "src/client/Main.cpp")
        paths += tuple(sorted((checkout / "src/diagnostics").glob("*.cpp")))
    report = {
        "schema": 1,
        "checkout": str(checkout),
        "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=checkout, text=True).strip(),
        "git_status": subprocess.check_output(["git", "status", "--porcelain"], cwd=checkout, text=True),
        "inputs_sha256": {str(path): digest(path) for path in paths},
        "private_source_sha256": digest(output / "RuntimeScene.owners.cpp"),
        "scope": "Stage-boundary CPU ownership/capacity only; not RSS, physical VRAM, GPU time or a frame gate.",
        "commands": [],
        "exit": None,
    }
    for name, command in (("compile", compile_command), ("link", link_command)):
        if command is None:
            continue
        start = time.monotonic()
        with (output / f"{name}.log").open("w") as log:
            result = subprocess.run(["nice", "-n", "10", *command], cwd=checkout,
                                    stdout=log, stderr=subprocess.STDOUT, check=False)
        report["commands"].append({"kind": name, "argv": command,
                                   "exit": result.returncode, "seconds": time.monotonic() - start})
        report["exit"] = result.returncode
        (output / "build.json").write_text(json.dumps(report, indent=2) + "\n")
        if result.returncode:
            raise SystemExit(f"{name} failed: see {output / (name + '.log')}")
    result_path = output / ("outshine-client-owners" if link_command else "RuntimeScene.owners.o")
    report["result_sha256"] = digest(result_path)
    (output / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(result_path)


if __name__ == "__main__":
    main()
