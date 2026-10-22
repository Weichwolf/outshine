"""Compare native triangle-pair selection and replay its coverage on the native GPU."""

import argparse
import importlib.util
import json
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parent
FIXED = SOURCE.parent / "fixed_coverage"


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
    output = args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty; existing evidence is preserved")
    checkout, database = (
        args.native_checkout.resolve(),
        args.compile_database.resolve(),
    )
    spec = importlib.util.spec_from_file_location(
        "fixed_coverage_runner", FIXED / "run.py"
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("fixed coverage build helper unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    shared = module.helpers()
    sources = [
        *sorted(SOURCE.glob("*.cpp")),
        *sorted(SOURCE.glob("*.h")),
        Path(__file__),
        FIXED / "FixedPatch.cpp",
        FIXED / "FixedPatch.h",
        FIXED / "UncertainPatch.cpp",
        FIXED / "IntegerUncertainty.cpp",
        FIXED / "UncertainPatch.h",
        SOURCE.parent / "vertex_projection/Envelope.cpp",
        SOURCE.parent / "vertex_projection/Envelope.h",
        SOURCE.parent / "vertex_projection/Input.h",
        FIXED / "Exact.h",
        FIXED / "run.py",
        Path(shared.__file__),
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
        "source_sha256": {str(p): shared.digest(p) for p in sources},
        "input_sha256": shared.digest(args.input.resolve()),
        "archive_sha256": shared.digest(checkout / "build/liboutshine.a"),
        "compile_database_sha256": shared.digest(database),
        "steps": steps,
        "success": False,
    }
    try:
        selector = output / "selector"
        selector.mkdir()
        binary = shared.build(
            checkout,
            database,
            selector,
            steps,
            sources=(
                SOURCE / "MeshPairs.cpp",
                SOURCE / "Probe.cpp",
                FIXED / "FixedPatch.cpp",
                FIXED / "UncertainPatch.cpp",
                FIXED / "IntegerUncertainty.cpp",
                SOURCE.parent / "vertex_projection/Envelope.cpp",
            ),
            cflags=shlex.split(
                subprocess.check_output(["pkg-config", "--cflags", "gmp"], text=True)
            ),
            libraries=shlex.split(
                subprocess.check_output(["pkg-config", "--libs", "gmp"], text=True)
            ),
        )
        receipt["selector_binary_sha256"] = shared.digest(binary)
        shared.execute(
            [
                binary,
                args.input.resolve(),
                output / "cases.json",
                output / "selection.json",
            ],
            checkout,
            output,
            "selection",
            steps,
        )
        receipt["generated_cases_sha256"] = shared.digest(output / "cases.json")
        shared.execute(
            [
                sys.executable,
                FIXED / "run.py",
                "--native-checkout",
                checkout,
                "--compile-database",
                database,
                "--cases",
                output / "cases.json",
                "--output",
                output / "gpu",
            ],
            checkout,
            output,
            "gpu",
            steps,
        )
        equivalence = []
        for name in (
            "native-first-edge",
            "native-aligned-normals",
            "native-uncertain",
            "native-uncertain-integer",
        ):
            for kind in ("colour", "depth"):
                original = output / "gpu" / f"{name}.{kind}.f32"
                reduced = output / "gpu" / f"{name}-reduced.{kind}.f32"
                equal = original.read_bytes() == reduced.read_bytes()
                equivalence.append(
                    {
                        "name": name,
                        "kind": kind,
                        "byte_identical": equal,
                        "original_sha256": shared.digest(original),
                        "reduced_sha256": shared.digest(reduced),
                    }
                )
                if not equal:
                    raise RuntimeError(f"reduction changed {name} {kind}")
        (output / "reduction-equivalence.json").write_text(
            json.dumps(equivalence, indent=2) + "\n"
        )
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
