"""Export dense physical rays for the native streaming/LUT probe, not runtime test pins."""

import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from air_path_cases import place_cases
from air_path_depth_table import depth_limit
from air_path_integral import engine_clear_air
from compare_air_depth_tables import load_reference

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--reference-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError("private ray fixtures belong outside the repository")
    report = load_reference(args.reference, args.reference_root)
    native_inputs = {}
    for name in report["inputs_sha256"]:
        old = (args.reference_root / name).read_bytes()
        current = (ROOT / name).read_bytes()
        if old != current:
            import_line = b"using std::floor;\n"
            if (
                name != "src/world/sky/Atmosphere.h"
                or current.replace(import_line, b"") != old
            ):
                raise ValueError(
                    f"native input changed beyond the standalone floor import: {name}"
                )
        native_inputs[name] = hashlib.sha256(current).hexdigest()
    medium = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")
    cases, _ = place_cases(ROOT)
    rays, queries, rows, descriptions = [], [], [], []
    for case in cases:
        height, view, sun, relative = case.arguments()
        view_sun = (
            view * sun + np.sqrt(1.0 - view**2) * np.sqrt(1.0 - sun**2) * relative
        )
        limit = depth_limit(medium, height, view, 200.0)
        ray = np.asarray(
            [medium.bottom_km + height, view, sun, view_sun, limit], dtype="<f4"
        )
        begin = len(queries)
        references = sorted(
            [row for row in report["references"] if row["case"] == case.name],
            key=lambda row: row["distance_km"],
        )
        for row in references:
            distance = min(float(ray[4]), float(np.float32(row["distance_km"])))
            for _ in range(2):
                queries.append(distance)
                rows.append(row)
        rays.append(struct.pack("<5f3I", *ray, begin, len(queries), 0))
        descriptions.append(
            {
                "case": case.record(),
                "ray_f32": ray.tolist(),
                "begin": begin,
                "end": len(queries),
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "rays.bin").write_bytes(b"".join(rays))
    np.asarray(queries, dtype="<f4").tofile(output / "queries.bin")
    metadata = {
        "scope": "Dense native FP32 streaming input; reference rows retain the original FP64 scenario coordinates. Quantization and native LUT error are included, not hidden by regenerating references.",
        "reference_sha256": hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        "native_inputs_sha256": native_inputs,
        "reference_inputs_sha256": report["inputs_sha256"],
        "ray_count": len(rays),
        "query_count": len(queries),
        "ray_stride_bytes": 32,
        "output_stride_bytes": 32,
        "descriptions": descriptions,
        "references": rows,
    }
    (output / "rays.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(
        json.dumps(
            {key: metadata[key] for key in ("scope", "ray_count", "query_count")}
        )
    )


if __name__ == "__main__":
    main()
