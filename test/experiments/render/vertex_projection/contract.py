"""Inspect scoped NoContraction on the frozen full Lit/Flat vertex variants."""

import argparse
import hashlib
import json
import re
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path

LIT_LAYOUTS = ("000", "010", "100", "110", "200", "210", "101", "111", "201", "211")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect(assembly, expected):
    decorated = set(re.findall(r"OpDecorate\s+(%\S+)\s+NoContraction", assembly))
    definitions = dict(re.findall(r"(%\S+)\s+=\s+(Op\w+)", assembly))
    operations = sorted(definitions.get(value, "missing") for value in decorated)
    wanted = (
        ["OpFAdd", "OpMatrixTimesVector", "OpMatrixTimesVector"] if expected else []
    )
    if operations != wanted:
        raise ValueError(f"unexpected precise projection operations: {operations}")
    if "RelaxedPrecision" in assembly:
        raise ValueError("this qualification requires full-precision source modules")
    widths = re.findall(r"OpTypeFloat\s+(\d+)", assembly)
    if widths != ["32"]:
        raise ValueError(f"unexpected floating-point types: {widths}")
    return {"no_contraction_operations": operations, "float_widths": widths}


def variants():
    for layout in LIT_LAYOUTS:
        for motion in range(2):
            yield (
                f"lit-{layout}-{motion}",
                "litVertex.glsl",
                [
                    f"-DLIT_UVS={layout[0]}",
                    f"-DLIT_TINTED={layout[1]}",
                    f"-DLIT_MAPPED={layout[2]}",
                    f"-DSURFACE_WRITES_VELOCITY={motion}",
                ],
            )
    for uv in range(3):
        for tint in range(2):
            for motion in range(2):
                yield (
                    f"flat-{uv}{tint}-{motion}",
                    "flatVertex.glsl",
                    [
                        f"-DFLAT_UVS={uv}",
                        f"-DFLAT_TINTED={tint}",
                        f"-DSURFACE_WRITES_VELOCITY={motion}",
                    ],
                )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-checkout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", type=Path)
    args = parser.parse_args()
    checkout, output = args.native_checkout.resolve(), args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty; existing evidence is preserved")
    compiler = args.compiler or Path(
        shutil.which("glslangValidator") or "glslangValidator"
    )
    compiler = compiler.resolve()
    if not compiler.is_file():
        parser.error(
            "glslangValidator is required; supply --compiler or its SDK environment"
        )
    steps, rows = [], []
    receipt = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
        ).strip(),
        "source_worktree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=Path(__file__).parent, text=True
            )
        ),
        "native_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=checkout, text=True
        ).strip(),
        "driver_sha256": digest(Path(__file__)),
        "compiler_sha256": digest(compiler),
        "source_sha256": {},
        "steps": steps,
        "rows": rows,
        "scope": "Full frozen Lit/Flat variants; annotation propagation only, no world/frame admission",
        "qualified_for_page_suppression": False,
        "success": False,
    }

    def execute(command):
        run = subprocess.run(
            [str(value) for value in command],
            cwd=checkout,
            check=False,
            capture_output=True,
            text=True,
        )
        steps.append({"command": list(map(str, command)), "returncode": run.returncode})
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        return run.stdout

    try:
        for name, filename, flags in variants():
            original = checkout / "src/render/shaders" / filename
            receipt["source_sha256"][str(original)] = digest(original)
            text = original.read_text()
            if text.count("void main() {") != 1 or text.count("gl_Position =") != 1:
                raise ValueError(
                    "source projection topology changed; inspect the new producer"
                )
            candidate = output / f"{name}.glsl"
            candidate.write_text(
                text.replace("void main() {", "precise gl_Position;\nvoid main() {")
            )
            binary = output / f"{name}.vert.spv"
            execute(
                [
                    "nice",
                    "-n",
                    "10",
                    compiler,
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    "--depfile",
                    output / f"{name}.d",
                    "-Isrc",
                    "-Isrc/content/shade",
                    "-Isrc/render/shaders",
                    "-S",
                    "vert",
                    *flags,
                    candidate,
                    "-o",
                    binary,
                ]
            )
            dependencies = (output / f"{name}.d").read_text().replace("\\\n", " ")
            for dependency in shlex.split(dependencies.partition(":")[2]):
                path = Path(dependency)
                if not path.is_absolute():
                    path = checkout / path
                receipt["source_sha256"][str(path)] = digest(path)
            execute(["spirv-val", "--target-env", "vulkan1.0", binary])
            precise = execute(["spirv-dis", binary])
            (output / f"{name}.spvasm").write_text(precise)
            existing = checkout / "build/shaders" / f"{name}.vert.spv"
            control = execute(["spirv-dis", existing])
            rows.append(
                {
                    "name": name,
                    "source_sha256": digest(original),
                    "generated_source_sha256": digest(candidate),
                    "cached_control_sha256": digest(existing),
                    "candidate_sha256": digest(binary),
                    "control": inspect(control, False),
                    "candidate": inspect(precise, True),
                }
            )
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
