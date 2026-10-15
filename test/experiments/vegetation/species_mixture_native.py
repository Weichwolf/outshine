"""Compare the actual native constant mixture with the height-mixture reference."""

import argparse
import hashlib
import json
import shlex
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import SpeciesHeightMixture

ROOT = Path(__file__).resolve().parents[3]


def build_probe(output):
    records = json.loads((ROOT / "compile_commands.json").read_text())
    record = next(row for row in records if row["file"].endswith("/SpeciesMixture.cpp"))
    arguments = shlex.split(record["command"])
    flags, at = [], 1
    while at < len(arguments):
        if arguments[at] in ("-o", "-MF", "-MT"):
            at += 2
            continue
        if arguments[at] not in ("-c", record["file"]):
            flags.append(arguments[at])
        at += 1
    binary = output / "species-mixture-probe"
    command = [
        "nice",
        "-n",
        "10",
        arguments[0],
        *flags,
        "test/experiments/vegetation/species_mixture_probe.cpp",
        "src/diagnostics/ProcessHeap.cpp",
        "build/libgenerators.a",
        "build/liboutshine.a",
        "-o",
        str(binary),
    ]
    (output / "build-command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (output / "build.log").open("w") as log:
        subprocess.run(
            command, cwd=ROOT, check=True, stdout=log, stderr=log, timeout=120
        )
    return binary


def fixtures():
    cases = [
        ("unordered", ["b", "a", "zero", "c"], [1, 1, 0, 2]),
        ("huge", ["a", "b", "c"], [1e308, 1e308, 0]),
        ("subnormal", ["a", "b", "c"], [5e-324, 1e-323, 0]),
        ("empty", ["a", "b", "c"], [0, 0, 0]),
    ]
    definitions = json.loads((ROOT / "src/assets/world/vegetation.json").read_text())
    catalogue = sorted(
        json.loads(path.read_text())["name"]
        for path in (ROOT / "src/assets/world/species").glob("*.json")
    )
    for entry in definitions["templates"]:
        weights = entry["trees"]["species"]
        cases.append(
            (entry["name"], catalogue, [weights.get(name, 0) for name in catalogue])
        )
    cases += [
        (name + "-reversed", names[::-1], weights[::-1])
        for name, names, weights in cases[:4]
    ]
    result, expected = [], []
    rng = np.random.default_rng(1977)
    for name, names, weights in cases:
        reference = SpeciesHeightMixture.prepare(names, (0, 1), (weights, weights))
        boundaries = reference.cumulative[0]
        draws = np.unique(
            np.r_[
                rng.random(4096),
                0,
                np.nextafter(1.0, 0),
                boundaries,
                np.nextafter(boundaries, 0),
                np.nextafter(boundaries, 1),
            ]
        )
        draws = draws[(draws >= 0) & (draws < 1)]
        rng.shuffle(draws)
        chosen = reference.choose(0, draws)
        expected.append(
            [
                -1 if index < 0 else names.index(reference.taxa[index])
                for index in chosen
            ]
        )
        result.append(
            {
                "name": name,
                "names": names,
                "weights": weights,
                "uniforms": draws.tolist(),
            }
        )
    return result, expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs require an explicit system-temp directory")
    output.mkdir(parents=True, exist_ok=True)
    binary = args.binary.resolve() if args.binary else build_probe(output)
    cases, expected = fixtures()
    source = output / "inputs.json"
    source.write_text(json.dumps({"cases": cases}, allow_nan=False) + "\n")
    native = subprocess.run(
        [str(binary), str(source)],
        capture_output=True,
        text=True,
        check=True,
        timeout=30,
    )
    (output / "native.json").write_text(native.stdout)
    (output / "native.stderr").write_text(native.stderr)
    actual = json.loads(native.stdout)["cases"]
    assert len(actual) == len(cases)
    report = []
    for case, wanted, got in zip(cases, expected, actual, strict=True):
        assert got["chosen"] == wanted, case["name"]
        if got["heap_instrumented"]:
            assert got["sampling_allocated_bytes"] == 0, case["name"]
            if any(weight > 0 for weight in case["weights"]):
                assert got["prepared_live_bytes"] > 0, case["name"]
        report.append(
            {
                "case": case["name"],
                "draws": len(wanted),
                "sampling_ns_per_draw": got["sampling_ns"] / len(wanted),
                "prepared_live_bytes": got["prepared_live_bytes"],
                "heap_instrumented": got["heap_instrumented"],
            }
        )
    receipt = {
        "scope": "constant declared mixtures; not height-dependent ecology, site hashing or a GPU/frame proof",
        "input_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)
        ),
        "archive_sha256": {
            name: hashlib.sha256((ROOT / "build" / name).read_bytes()).hexdigest()
            for name in ("libgenerators.a", "liboutshine.a")
        },
        "cases": report,
    }
    (output / "comparison.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(
        json.dumps({"cases": len(report), "draws": sum(row["draws"] for row in report)})
    )


if __name__ == "__main__":
    main()
