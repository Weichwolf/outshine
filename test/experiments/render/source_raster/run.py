"""Audit original triangle raster depth against exact-input inner patch bounds."""

import argparse
import importlib.util
import json
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-checkout", required=True, type=Path)
    parser.add_argument("--compile-database", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seed", type=int, default=1831565813)
    parser.add_argument("--isolate-case", type=int, action="append", default=[])
    args = parser.parse_args()
    if not 0 <= args.seed <= 0xFFFFFFFF:
        parser.error("seed must be a uint32")
    if any(not 0 <= index < 4096 for index in args.isolate_case):
        parser.error("case index must be in [0, 4096)")
    checkout, output = args.native_checkout.resolve(), args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty; existing evidence is preserved")
    helper = SOURCE.parent / "conservative_coverage/run.py"
    spec = importlib.util.spec_from_file_location("coverage_build", helper)
    if spec is None or spec.loader is None:
        raise RuntimeError("native build helper unavailable")
    sys.dont_write_bytecode = True
    shared = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(shared)
    fixed = SOURCE.parent / "fixed_coverage"
    sources = [
        *sorted(SOURCE.glob("*.cpp")),
        *sorted(SOURCE.glob("*.h")),
        SOURCE / "source.vert",
        *sorted(SOURCE.glob("*.py")),
        fixed / "FixedPatch.cpp",
        fixed / "FixedPatch.h",
        fixed / "Exact.h",
        helper,
    ]
    steps = []
    receipt = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True
        ).strip(),
        "source_worktree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=SOURCE, text=True
            )
        ),
        "native_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=checkout, text=True
        ).strip(),
        "sources_sha256": {str(path): shared.digest(path) for path in sources},
        "archive_sha256": shared.digest(checkout / "build/liboutshine.a"),
        "compile_database_sha256": shared.digest(args.compile_database.resolve()),
        "seed": args.seed,
        "isolated_cases": args.isolate_case,
        "scope": "Counterexample audit; terminal success does not assert that the observed bound holds",
        "steps": steps,
        "success": False,
    }
    try:
        shared.execute(
            [
                "nice",
                "-n",
                "10",
                "glslangValidator",
                "-V",
                "--target-env",
                "vulkan1.3",
                SOURCE / "source.vert",
                "-o",
                output / "source.vert.spv",
            ],
            checkout,
            output,
            "shader",
            steps,
        )
        shared.execute(
            ["spirv-val", "--target-env", "vulkan1.3", output / "source.vert.spv"],
            checkout,
            output,
            "shader-validation",
            steps,
        )
        binary = shared.build(
            checkout,
            args.compile_database.resolve(),
            output,
            steps,
            sources=(
                SOURCE / "Cases.cpp",
                SOURCE / "Probe.cpp",
                fixed / "FixedPatch.cpp",
            ),
            cflags=(
                f"-I{fixed}",
                *shlex.split(
                    subprocess.check_output(["pkg-config", "--cflags", "gmp"], text=True)
                ),
            ),
            libraries=shlex.split(
                subprocess.check_output(["pkg-config", "--libs", "gmp"], text=True)
            ),
        )
        receipt["binary_sha256"] = shared.digest(binary)
        receipt["shader_sha256"] = shared.digest(output / "source.vert.spv")
        shared.execute(
            ["nice", "-n", "10", binary, output, str(args.seed), "4096", "0"],
            checkout,
            output,
            "run",
            steps,
        )
        for index in args.isolate_case:
            isolated = output / f"isolated-{index}"
            isolated.mkdir()
            shutil.copyfile(output / "source.vert.spv", isolated / "source.vert.spv")
            shared.execute(
                ["nice", "-n", "10", binary, isolated, str(args.seed), str(index), "0"],
                checkout,
                output,
                f"isolated-{index}",
                steps,
            )
        normalized = output / "normalized"
        normalized.mkdir()
        shutil.copyfile(output / "source.vert.spv", normalized / "source.vert.spv")
        shared.execute(
            ["nice", "-n", "10", binary, normalized, str(args.seed), "4096", "1"],
            checkout,
            output,
            "normalized",
            steps,
        )
        shared.execute(
            [
                "python3",
                SOURCE / "verify.py",
                output,
                "--output",
                output / "verification.json",
            ],
            checkout,
            output,
            "verification",
            steps,
        )
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
