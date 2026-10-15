"""Exercise actual perspective interpolation for the native facade phase fix."""

import argparse
import hashlib
import io
import json
import tempfile
from pathlib import Path

import numpy as np
import shapely
from facade_phase_raster import (
    ROOT,
    VERTEX,
    VERTEX_OFFSET,
    capture,
    compile_pairs,
    payload,
    run,
)


def projective(data):
    data = bytearray(data)
    view = np.frombuffer(data, dtype="<f4", count=56).copy()
    matrix = view[:16].reshape(4, 4).T.copy()
    row = np.frombuffer(data, dtype="<f4", count=16, offset=608).reshape(4, 4).T
    vertices = np.frombuffer(data, dtype=VERTEX, count=6, offset=VERTEX_OFFSET)
    world = np.column_stack([vertices["p"], np.ones(6)]) @ row.T
    world[:, :3] += view[48:51]
    before = world @ matrix.T
    matrix[3] += 0.25 / np.max(np.abs(before[:, 0])) * matrix[0]
    clip = world @ matrix.T
    assert clip[:, 3].min() >= 0.74 and np.ptp(clip[:, 3]) > 0.25
    projected = clip[:, :2] / clip[:, 3:4]
    assert shapely.Polygon(projected[[0, 1, 2, 5]]).covers(shapely.box(-1, -1, 1, 1))
    view[:16] = matrix.T.ravel()
    view[16:32] = matrix.T.ravel()
    data[:224] = view.tobytes()
    return data, [float(clip[:, 3].min()), float(clip[:, 3].max())]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    offsets = compile_pairs(output, args.baseline.resolve())
    frame = np.loadtxt(
        io.StringIO(run([str(args.binary.resolve()), "--frame"], output, "frame"))
    )
    records = []
    for style in (1, 8, 7):
        for field in (0, 1, 2):
            images = {mode: [] for mode in ("old", "new")}
            for variant in range(8):
                data = payload(
                    frame, offsets, style, field, variant, 1, 10, [0, 0, 0], [0, 0, 0]
                )
                data, clip_w = projective(data)
                for mode, captured in images.items():
                    captured.append(
                        capture(
                            args.binary.resolve(),
                            output,
                            f"s{style}-f{field}-v{variant}",
                            mode,
                            data,
                        )
                    )
            errors = {
                mode: float(
                    max(
                        np.max(np.abs(pixels[1:] - captured[0][1:]))
                        for pixels in captured[1:]
                    )
                )
                for mode, captured in images.items()
            }
            assert errors["new"] < 2e-4, (style, field, errors)
            records.append(
                {
                    "style": style,
                    "field": field,
                    "clip_w_range": clip_w,
                    "max_metadata_error": errors,
                }
            )
    assert max(r["max_metadata_error"]["old"] for r in records) > 0.4
    sources = [Path(__file__), Path(__file__).with_name("facade_phase_raster.py")]
    report = {
        "scope": __doc__,
        "native_raster": True,
        "captures": 144,
        "cases": records,
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sources
        },
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "native_raster": True,
                "captures": 144,
                "max_fixed_error": max(r["max_metadata_error"]["new"] for r in records),
            }
        )
    )


if __name__ == "__main__":
    main()
