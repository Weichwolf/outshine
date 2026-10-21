"""Measure the native position expressions in an isolated native vertex-stage probe."""

import argparse
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parent


def block(text, begin, end):
    first = text.index(begin)
    last = text.index(end, first)
    result = text[first:last].rstrip()
    if "gl_Position =" not in result:
        raise ValueError("native position block changed")
    return result


def float32_controls(shared, checkout, output, steps):
    binary = output / "projection.vert.spv"
    assembly = output / "projection.vert.spvasm"
    shared.execute(
        ["spirv-dis", binary, "-o", assembly],
        checkout,
        output,
        "float32-disassemble",
        steps,
    )
    lines = assembly.read_text().splitlines()
    entries = [line.split()[2] for line in lines if "OpEntryPoint " in line]
    if len(entries) != 1:
        raise ValueError("float32 controls require exactly one shader entry point")
    modes = ("DenormPreserve", "SignedZeroInfNanPreserve", "RoundingModeRTE")
    first = next(
        i
        for i, line in enumerate(lines)
        if "OpExtInstImport " in line or "OpMemoryModel " in line
    )
    lines[first:first] = [f"               OpCapability {mode}" for mode in modes]
    first = next(i for i, line in enumerate(lines) if "OpEntryPoint " in line) + 1
    lines[first:first] = [
        f"               OpExecutionMode {entries[0]} {mode} 32" for mode in modes
    ]
    assembly.write_text("\n".join(lines) + "\n")
    shared.execute(
        ["spirv-as", "--target-env", "vulkan1.3", assembly, "-o", binary],
        checkout,
        output,
        "float32-assemble",
        steps,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-checkout", required=True, type=Path)
    parser.add_argument("--compile-database", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--spirv-float32-controls",
        action="store_true",
        help="Vulkan-only vertex RTE/denormal/signed-zero preservation comparison",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=SOURCE.parents[3]
        / "test/outshine/data/render/patch_selection/native-mesh-subset.json",
    )
    args = parser.parse_args()
    if args.spirv_float32_controls and sys.platform != "linux":
        parser.error("SPIR-V float32 controls are measured only with Vulkan on Linux")
    checkout, output = args.native_checkout.resolve(), args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty; existing evidence is preserved")
    helper_path = SOURCE.parent / "conservative_coverage/run.py"
    spec = importlib.util.spec_from_file_location("coverage_build", helper_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("native coverage build helper unavailable")
    shared = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(shared)
    lit = checkout / "src/render/shaders/litVertex.glsl"
    flat = checkout / "src/render/shaders/flatVertex.glsl"
    sources = [
        SOURCE / "Probe.cpp",
        SOURCE / "projection.vert.in",
        SOURCE / "projection.frag",
        Path(__file__),
        lit,
        flat,
        helper_path,
    ]
    steps = []
    receipt = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True
        ).strip(),
        "native_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=checkout, text=True
        ).strip(),
        "sources_sha256": {str(p): shared.digest(p) for p in sources},
        "input_sha256": shared.digest(args.input.resolve()),
        "archive_sha256": shared.digest(checkout / "build/liboutshine.a"),
        "compile_database_sha256": shared.digest(args.compile_database.resolve()),
        "spirv_float32_controls": args.spirv_float32_controls,
        "steps": steps,
        "success": False,
    }
    try:
        vertex = (SOURCE / "projection.vert.in").read_text()
        vertex = vertex.replace(
            "@LIT_POSITION@",
            block(
                lit.read_text(),
                "  mat4 m = placement.current;",
                "  position = world.xyz;",
            ),
        )
        vertex = vertex.replace(
            "@FLAT_POSITION@",
            block(
                flat.read_text(),
                "  mat4 placement = placed.current;",
                "  uv = vec2(0.0);",
            ),
        )
        (output / "projection.vert").write_text(vertex)
        for name, source in [
            ("projection.vert", output / "projection.vert"),
            ("projection.frag", SOURCE / "projection.frag"),
        ]:
            shared.execute(
                [
                    "nice",
                    "-n",
                    "10",
                    "glslangValidator",
                    "-V",
                    "--target-env",
                    "vulkan1.3",
                    source,
                    "-o",
                    output / (name + ".spv"),
                ],
                checkout,
                output,
                name,
                steps,
            )
        if args.spirv_float32_controls:
            float32_controls(shared, checkout, output, steps)
        for name in ("projection.vert", "projection.frag"):
            shared.execute(
                ["spirv-val", "--target-env", "vulkan1.3", output / (name + ".spv")],
                checkout,
                output,
                name + "-validation",
                steps,
            )
        receipt["generated_vertex_sha256"] = shared.digest(output / "projection.vert")
        receipt["vertex_spirv_sha256"] = shared.digest(output / "projection.vert.spv")
        binary = shared.build(
            checkout,
            args.compile_database.resolve(),
            output,
            steps,
            sources=(SOURCE / "Probe.cpp",),
        )
        compile_entry = json.loads((output / "compile_commands.json").read_text())[0]
        flags = compile_entry["arguments"]
        contract_flags = [flag for flag in flags if flag.startswith("-ffp-contract=")]
        unsafe = {"-ffast-math", "-funsafe-math-optimizations", "-fassociative-math"}
        if (
            not contract_flags
            or contract_flags[-1] != "-ffp-contract=off"
            or unsafe.intersection(flags)
        ):
            raise RuntimeError(
                "CPU reference needs ordered uncontracted arithmetic in its actual CDB"
            )
        receipt["cpu_reference_contraction_disabled"] = True
        receipt["binary_sha256"] = shared.digest(binary)
        environment = (
            dict(os.environ, MTL_DEBUG_LAYER="1") if sys.platform == "darwin" else None
        )
        shared.execute(
            [binary, output, args.input.resolve(), output / "result.json"],
            checkout,
            output,
            "run",
            steps,
            environment=environment,
        )
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
