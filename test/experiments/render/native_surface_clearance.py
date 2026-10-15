"""Compare the native public clearance API with unchanged transport mesh pins."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "test/experiments/transport/reference/height-gap"
OUTPUT = ROOT / "build/experiments/render/native-surface-clearance"


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    environment = dict(os.environ, PYTHONPATH=str(ROOT / "lib/src"))
    subprocess.run(
        [
            sys.executable,
            ROOT / "test/experiments/transport/verify_height_gap_reference.py",
        ],
        cwd=ROOT,
        timeout=60,
        env=environment,
        check=True,
    )
    metadata = json.loads((REFERENCE / "metadata.json").read_text())
    pins = json.loads((REFERENCE / "pins.json").read_text())["results"]
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / "proof.json").unlink(missing_ok=True)
    native_input = OUTPUT / "pairs.txt"
    with (
        np.load(REFERENCE / "pairs.npz", allow_pickle=False) as arrays,
        native_input.open("w") as output,
    ):
        output.write(f"{len(pins)}\n")
        for case, pin in zip(metadata["cases"], pins, strict=True):
            assert case["name"] == pin["name"]
            output.write(f"{case['name']} {pin['minimum_clearance_m']:.17g}\n")
            for role in ("lower", "upper"):
                spec = case[role]["arrays"]
                xyz = arrays[spec["vertices"]["key"]]
                faces = arrays[spec["faces"]["key"]]
                output.write(f"{len(xyz)} {len(faces)}\n")
                np.savetxt(output, xyz, fmt="%.17g")
                np.savetxt(output, faces, fmt="%u")
    began = time.perf_counter()
    result = subprocess.run(
        [binary, native_input],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
        timeout=60,
    )
    wall_ms = (time.perf_counter() - began) * 1000
    (OUTPUT / "native-result.txt").write_text(result.stdout + result.stderr)
    result.check_returncode()
    report = {
        "pairs": len(pins),
        "pairs_sha256": metadata["pairs_sha256"],
        "pins_sha256": metadata["pins_sha256"],
        "native_binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "native_input_sha256": hashlib.sha256(native_input.read_bytes()).hexdigest(),
        "minimum_and_attained_witness_tolerance_m": 1e-7,
        "frame": metadata["frame"],
        "binding_scope": metadata["binding_scope"],
        "native_process_wall_ms": wall_ms,
        "scope": "Affine surfaces only; process wall time is no frame/device/GPU proof.",
    }
    (OUTPUT / "proof.json").write_text(json.dumps(report, indent=2) + "\n")
    print(result.stdout.strip())
    print(json.dumps({"pairs": len(pins), "native_process_wall_ms": wall_ms}))


if __name__ == "__main__":
    main()
