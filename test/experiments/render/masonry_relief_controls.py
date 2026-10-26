"""Independent native edge/period and existing shading-normal composition controls."""

import numpy as np
from facade_coating_raster import ROOT
from ground_litter_native import capture
from masonry_relief_filter import normal
from masonry_relief_reference import exact


def angle(a, b):
    return np.rad2deg(2 * np.arcsin(np.minimum(np.linalg.norm(a - b, axis=-1) / 2, 1)))


def periodic(binary, output, compile_shader):
    destination = output / "period-controls"
    destination.mkdir(exist_ok=True)
    rows = []
    for axis, period in ((0, 0.25), (1, 0.16)):
        for span in (0.99, 0.995, 0.999, 1, 1.001, 1.005, 1.01, 2, 4):
            for position in (0.0041, 0.005, 0.0751, 0.1599, 0.2449):
                row = np.zeros(16, dtype="<f4")
                row[:4] = [0.1, 0.1, 0.12, 0.04]
                row[2 + axis] = position
                row[6], row[11] = (
                    (span * period, 1e-6) if axis == 0 else (1e-6, span * period)
                )
                row[15] = 0.002
                rows.append(row)
    records = np.array(rows).astype(float)
    prefix = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    palette = np.zeros(320 + records.size, dtype="<f4")
    palette[: len(prefix)] = prefix
    palette[320:] = records.ravel()
    palette.tofile(destination / "native.palette")
    probes = np.zeros(
        len(rows), dtype=[("v", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    probes["which"] = np.arange(len(rows))
    probes.tofile(destination / "probes.bin")
    reference = np.array(
        [
            exact(row[:4], row[4:8], row[8:12], row[12:14], row[14], depth=row[15])[0]
            for row in records
        ]
    )
    reference_normal = normal(reference, records[:, 6:8], records[:, 10:12])
    results = []
    for side in (1, 4):
        compile_shader(destination, side)
        actual = capture(binary, destination, side, "native", len(rows), 13)
        gradient_error = float(np.abs(actual[:, :2] - reference).max())
        normal_error = float(angle(actual[:, 4:7], reference_normal).max())
        assert gradient_error < 0.0002 and normal_error < 0.05, (
            side,
            gradient_error,
            normal_error,
        )
        results.append(
            {
                "side": side,
                "cases": len(rows),
                "gradient_error": gradient_error,
                "normal_error_deg": normal_error,
            }
        )
    return results


def authored_composition(binary, output, records, ordinary, compile_shader):
    compile_shader(output, 4, authored=True)
    actual = capture(binary, output, "4-authored", "native", len(records), 13)
    np.testing.assert_array_equal(actual[:, :4], ordinary[:, :4])
    beta = np.array([0.23, -0.17])
    expected = normal(
        ordinary[:, :2], records[:, 6:8], records[:, 10:12], authored=beta
    )
    error = float(angle(actual[:, 4:7], expected).max())
    assert error < 0.05, error
    return {
        "cases": len(records),
        "max_normal_error_deg": error,
        "height_derivatives_byte_equal": True,
        "scope": "Actual bumpNormal with an authored shading vector, not a texture-loader test",
    }
