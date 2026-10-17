"""Compare native recipe streams with NumPy PCG64 without generating test oracles."""

import argparse
import json
import random
import subprocess
from pathlib import Path

import numpy as np


def check(binary, output):
    edge = [0, 1, 17, 42, 2**32 - 1, 2**32, 2**32 + 1, 2**63, 2**64 - 1]
    cases = [(seed, False, 0) for seed in edge]
    cases += [(seed, True, axis) for seed in edge for axis in edge]
    source = random.Random(17)
    cases += [
        (source.getrandbits(64), bool(at % 2), source.getrandbits(64))
        for at in range(512)
    ]
    result = subprocess.run(
        [str(binary)],
        input="".join(f"{seed} {int(paired)} {axis}\n" for seed, paired, axis in cases),
        text=True,
        capture_output=True,
        check=True,
        timeout=10,
    )
    output.mkdir(parents=True, exist_ok=True)
    (output / "native.log").write_text(result.stdout + result.stderr)
    maximum = 0.0
    for (seed, paired, axis), line in zip(
        cases, result.stdout.splitlines(), strict=True
    ):
        entropy = np.random.SeedSequence((seed, axis)) if paired else seed
        stream = np.random.PCG64(entropy)
        fields = line.split()
        if list(map(int, fields[:64])) != list(map(int, stream.random_raw(64))):
            raise ValueError(f"raw recipe stream changed for seed={seed}, axis={axis}")
        expected = np.random.Generator(stream).uniform(-np.pi, np.pi, 4)
        error = float(
            np.max(np.abs(np.array(list(map(float, fields[64:]))) - expected))
        )
        if error > 2e-15:
            raise ValueError(f"uniform recipe interval changed by {error}")
        maximum = max(maximum, error)
    receipt = {
        "cases": len(cases),
        "raw_words": len(cases) * 64,
        "raw_exact": True,
        "uniform_max_error": maximum,
        "numpy": np.__version__,
        "scope": "recipe randomness only; no generated plant, world or device acceptance",
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check(args.binary.resolve(), args.output.resolve())


if __name__ == "__main__":
    main()
