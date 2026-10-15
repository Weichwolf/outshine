"""Compare native LUT resolution, midpoint integration and FP16 storage in isolation."""

import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import numpy as np
from air_path_integral import boundary_distance, engine_clear_air, transmittance

ROOT = Path(__file__).resolve().parents[3]


def write_texture(path, pixels, width, height, half=False):
    data = pixels.astype("<f2").astype("<f4") if half else pixels.astype("<f4")
    path.write_bytes(struct.pack("<2I", width, height) + data.tobytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=256)
    parser.add_argument("--height", type=int, default=64)
    args = parser.parse_args()
    output = args.output.resolve()
    if ROOT == output or ROOT in output.parents:
        raise ValueError("private LUT inputs belong outside the repository")
    if not 1 <= args.width <= 2048 or not 1 <= args.height <= 2048:
        raise ValueError("LUT dimensions exceed the experiment budget")
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [str(args.exporter.resolve()), str(output), str(args.width), str(args.height)],
        check=True,
    )
    native = np.frombuffer(
        (output / "veil-native-f32.bin").read_bytes(), dtype="<f4", offset=8
    ).reshape(-1, 4)
    write_texture(
        output / "veil-native-f16.bin", native, args.width, args.height, half=True
    )
    medium = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")
    looks = (
        np.fromfile(output / "looks.bin", dtype="<f4").reshape(-1, 2).astype(np.float64)
    )
    accurate = np.ones((len(looks), 4), dtype="<f4")
    refinement = 0.0
    for start in range(0, len(looks), 1024):
        batch = looks[start : start + 1024]
        reach = boundary_distance(batch[:, 0], batch[:, 1], medium.top_km)
        values = transmittance(medium, batch[:, 0], batch[:, 1], reach, 32)
        refined = transmittance(medium, batch[:, 0], batch[:, 1], reach, 64)
        refinement = max(refinement, float(np.max(np.abs(values - refined))))
        accurate[start : start + len(batch), :3] = values
    write_texture(output / "veil-accurate-f32.bin", accurate, args.width, args.height)
    write_texture(
        output / "veil-accurate-f16.bin", accurate, args.width, args.height, half=True
    )
    report = {
        "scope": "CPU native MediumTransmittance/40 midpoint equations, not a captured GPU runtime LUT. R32 sampled textures carry F32 or FP16-rounded values. Clear gases, Psi=0; native Mie/MS convergence remains open.",
        "width": args.width,
        "height": args.height,
        "runtime_rgba16f_bytes_if_selected": args.width * args.height * 8,
        "maximum_accurate_32_to_64_absolute_T_refinement": refinement,
        "medium_sha256": hashlib.sha256(
            (output / "medium.bin").read_bytes()
        ).hexdigest(),
        "files_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in output.glob("*.bin")
        },
    }
    (output / "lut-inputs.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {key: value for key, value in report.items() if key != "files_sha256"}
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
