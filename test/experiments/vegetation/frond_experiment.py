"""Verify complete guide-deformed organs, compare costs and prepare current previews."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from axis_surface import axis_mesh
from frond_checks import check_differentials, check_mesh, check_stability, guide_cases
from frond_pattern import PinnatePattern
from frond_scenes import make_scenes


def costs(output, name, guide, source, expected):
    mesh_path, guide_path = output / f"{name}-source.npz", output / f"{name}-guide.json"
    np.savez_compressed(mesh_path, **source)
    guide_path.write_text(json.dumps(guide.recipe(), indent=2) + "\n")
    digests = {
        key: hashlib.sha256(value.tobytes()).hexdigest()
        for key, value in expected.items()
    }
    samples = {method: [] for method in ("scalar", "batch")}
    for _ in range(3):
        for method, runs in samples.items():
            command = [
                "/bin/sh",
                "-c",
                '"$@"; frond_exit=$?; exit "$frond_exit"',
                "frond-cost",
                sys.executable,
                str(Path(__file__).with_name("frond_cost.py")),
                "--mesh",
                str(mesh_path),
                "--guide",
                str(guide_path),
                "--method",
                method,
            ]
            sample = json.loads(subprocess.check_output(command, text=True))
            assert sample["sha256"] == digests
            runs.append(sample)
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/fronds")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    stability = check_stability()
    began = time.perf_counter()
    source = PinnatePattern().mesh()
    source_s = time.perf_counter() - began
    cases = []
    for name, guide in zip(("flat", "folded", "curved", "profiled"), guide_cases()):
        error = check_differentials(guide)
        result = check_mesh(guide, source)
        check_mesh(guide, axis_mesh(guide.length, 0, 0.004, 0.0007, 96, 8))
        samples = costs(args.output, name, guide, source, result)
        cases.append(
            {
                "name": name,
                "jacobian_error": error,
                "samples": samples,
                "vertices": len(source["vertices"]),
                "triangles": len(source["faces"]),
                "source_bytes": sum(value.nbytes for value in source.values()),
                "product_bytes": sum(value.nbytes for value in result.values()),
                "identity": guide.identity,
            }
        )
    make_scenes(args.output)
    files = [
        Path(__file__).with_name(name)
        for name in (
            "frond_experiment.py",
            "frond_surface.py",
            "frond_pattern.py",
            "frond_checks.py",
            "frond_cost.py",
            "frond_scenes.py",
        )
    ]
    report = {
        "stability": stability,
        "source_s": source_s,
        "cases": cases,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "original_modeler_equivalence_proven": False,
    }
    (args.output / "fronds.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({"cases": len(cases), "source_s": source_s, "stability": stability})
    )


if __name__ == "__main__":
    main()
