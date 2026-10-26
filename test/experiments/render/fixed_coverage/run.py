"""Run the exact-geometry/fixed-grid native coverage experiment."""

import argparse
import importlib.util
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parent


def helpers():
    path = SOURCE.parent / "conservative_coverage/run.py"
    spec = importlib.util.spec_from_file_location("native_coverage_build", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("native coverage build helper unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-checkout", required=True, type=Path)
    parser.add_argument("--compile-database", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cases", type=Path)
    args = parser.parse_args()
    checkout = args.native_checkout.resolve()
    output = args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty; existing evidence is preserved")
    database = (args.compile_database or checkout / "compile_commands.json").resolve()
    shared = helpers()
    commands = []
    fixed_cases = (
        SOURCE.parents[3]
        / "test/outshine/data/render/fixed_coverage/uncertain-patches.json"
    )
    sources = [
        fixed_cases,
        *sorted(SOURCE.glob("*.cpp")),
        *sorted(SOURCE.glob("*.h")),
        *sorted(SOURCE.glob("*.glsl")),
        SOURCE / "patch.vert",
        SOURCE / "patch.frag",
        Path(__file__),
        Path(shared.__file__),
        SOURCE.parent / "vertex_projection/Envelope.h",
        SOURCE.parent / "vertex_projection/Input.h",
    ]
    receipt = {
        "native_checkout": str(checkout),
        "native_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=checkout, text=True
        ).strip(),
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True
        ).strip(),
        "source_worktree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=SOURCE, text=True
            )
        ),
        "sources_sha256": {str(path): shared.digest(path) for path in sources},
        "archive_sha256": shared.digest(checkout / "build/liboutshine.a"),
        "compile_database_sha256": shared.digest(database),
        "gmp_version": subprocess.check_output(
            ["pkg-config", "--modversion", "gmp"], text=True
        ).strip(),
        "steps": commands,
        "success": False,
    }
    if args.cases:
        receipt["case_input"] = {
            "path": str(args.cases.resolve()),
            "sha256": shared.digest(args.cases.resolve()),
        }
    try:
        compiler = checkout / "build/deps/install/bin/glslangValidator"
        if not compiler.is_file():
            compiler = shutil.which("glslangValidator")
        if not compiler:
            raise RuntimeError("glslangValidator is required")
        for name in ("patch.vert", "patch.frag"):
            shared.execute(
                [
                    "nice",
                    "-n",
                    "10",
                    str(compiler),
                    "-V",
                    "--target-env",
                    "vulkan1.3",
                    "-I" + str(SOURCE),
                    str(SOURCE / name),
                    "-o",
                    str(output / (name + ".spv")),
                ],
                checkout,
                output,
                name,
                commands,
            )
        binary = shared.build(
            checkout,
            database,
            output,
            commands,
            sources=(
                SOURCE / "FixedPatch.cpp",
                SOURCE / "UncertainPatch.cpp",
                SOURCE / "IntegerUncertainty.cpp",
                SOURCE / "Probe.cpp",
                SOURCE / "CaseInput.cpp",
            ),
            cflags=[
                "-I" + str(SOURCE.parent),
                *shlex.split(
                    subprocess.check_output(
                        ["pkg-config", "--cflags", "gmp"], text=True
                    )
                ),
            ],
            libraries=shlex.split(
                subprocess.check_output(["pkg-config", "--libs", "gmp"], text=True)
            ),
        )
        receipt["binary_sha256"] = shared.digest(binary)
        environment = None
        if sys.platform == "darwin":
            environment = dict(os.environ, MTL_DEBUG_LAYER="1")
        command = [binary, output, output / "result.json"]
        command.append(str(fixed_cases))
        if args.cases:
            command.append(str(args.cases.resolve()))
        shared.execute(
            command,
            checkout,
            output,
            "run",
            commands,
            environment=environment,
        )
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
