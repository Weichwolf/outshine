"""Compare projection arithmetic in indexed depth draws of the frozen native meshes."""

import argparse
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import run as projection

SOURCE = Path(__file__).resolve().parent


def cost_source(checkout):
    vertex = projection.vertex_source(checkout)
    changes = {
        "layout(location = 0) flat out vec4 measured;": "layout(location = 0) in vec3 sourcePoint;",
        "vec3 p = s.point.xyz;": "vec3 p = sourcePoint;",
    }
    for old, new in changes.items():
        if vertex.count(old) != 1:
            raise ValueError("projection input template changed")
        vertex = vertex.replace(old, new)
    tail = "  measured = gl_Position;"
    if vertex.count(tail) != 1:
        raise ValueError("projection transport template changed")
    return vertex[: vertex.index(tail)] + "}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-checkout", required=True, type=Path)
    parser.add_argument("--compile-database", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--input",
        type=Path,
        default=SOURCE.parents[3]
        / "test/outshine/data/render/patch_selection/native-mesh-subset.json",
    )
    args = parser.parse_args()
    checkout, output = args.native_checkout.resolve(), args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty; existing evidence is preserved")
    helper_path = SOURCE.parent / "conservative_coverage/run.py"
    spec = importlib.util.spec_from_file_location("coverage_build", helper_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("native build helper unavailable")
    shared = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(shared)
    sources = [
        SOURCE / name
        for name in ("Cost.cpp", "Support.h", "projection.vert.in", "run.py", "cost.py")
    ]
    sources += [
        checkout / "src/render/shaders" / name
        for name in ("litVertex.glsl", "flatVertex.glsl")
    ]
    sources.append(helper_path)
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
        "diagnostic_environment": {
            name: os.environ.get(name)
            for name in ("INTEL_DEBUG", "MESA_SHADER_CACHE_DISABLE")
        },
        "load_average_start": os.getloadavg(),
        "steps": steps,
        "success": False,
    }
    try:
        vertex = cost_source(checkout)
        receipt["shaders_sha256"] = {}
        for mode in range(4):
            shader = output / f"cost-{mode}.vert"
            shader.write_text(vertex.replace("s.options.x", f"{mode}u"))
            binary = output / (shader.name + ".spv")
            shared.execute(
                [
                    "nice",
                    "-n",
                    "10",
                    "glslangValidator",
                    "-V",
                    "--target-env",
                    "vulkan1.3",
                    shader,
                    "-o",
                    binary,
                ],
                checkout,
                output,
                f"shader-{mode}",
                steps,
            )
            shared.execute(
                ["spirv-val", "--target-env", "vulkan1.3", binary],
                checkout,
                output,
                f"validation-{mode}",
                steps,
            )
            receipt["shaders_sha256"][str(binary)] = shared.digest(binary)
        binary = shared.build(
            checkout,
            args.compile_database.resolve(),
            output,
            steps,
            sources=(SOURCE / "Cost.cpp",),
        )
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
        receipt["load_average_end"] = os.getloadavg()
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
