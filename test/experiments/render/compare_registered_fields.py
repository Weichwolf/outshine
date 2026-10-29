"""Native whole-original union capture on one explicit raster before GPU reduction."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np
from export_field_backgrounds import native, read
from field_background_inputs import ATTRIBUTES, digest, original_inputs, stage_mesh

FIXTURE = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/building-field-composition"
)


def compare(arrays, intervals):
    foreground = arrays["foreground_colour"]
    depth = arrays["foreground_depth"]
    result = {}
    for mode in ("ordered", "interleaved"):
        other = arrays[mode + "-background_colour"]
        far = arrays[mode + "-background_depth"]
        joint = arrays[mode + "-union_colour"]
        common = (depth > 0) & (far > 0)
        nearest = np.where((depth >= far)[:, :, None], foreground, other)
        if not np.array_equal(nearest, joint) or not np.array_equal(
            np.maximum(depth, far), arrays[mode + "-union_depth"]
        ):
            raise ValueError(
                "native joint capture differs from nearest original point hits"
            )
        before = int(np.count_nonzero(common & (depth > far)))
        after = int(np.count_nonzero(common & (depth < far)))
        separated = intervals["foreground"][1] < intervals[mode + "-background"][0]
        if not before or (mode == "ordered" and (after or not separated)):
            raise ValueError("whole originals do not exhibit required ordered control")
        if mode == "interleaved" and not after:
            raise ValueError("interleave needs actual native hits in both orders")
        over = foreground + (1 - foreground[:, :, 3, None]) * other
        error = float(np.abs(over - joint).max())
        if (mode == "ordered" and error != 0) or (mode == "interleaved" and error == 0):
            raise ValueError("global order did not distinguish the two controls")
        result[mode] = {
            "foreground_first": before,
            "background_first": after,
            "native_point_colour_and_depth_exact": True,
            "whole_forward_intervals_separated": bool(separated),
            "global_foreground_over_error": error,
        }
    return result


def run(args):
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if output.exists() or output == temporary or not output.is_relative_to(temporary):
        raise ValueError("fresh explicit system-temp output required")
    fixture = json.loads((FIXTURE / "manifest.json").read_text())
    camera = fixture["capture_camera"]
    eye, forward, right = (np.asarray(camera[k]) for k in ("eye_m", "forward", "right"))
    basis = np.asarray([camera[k] for k in ("right", "up", "forward")])
    query = {
        "name": "base",
        "capture": camera,
        "sun_elevation_deg": 45.0,
        "basis": basis.tolist(),
    }
    parts, source, _ = original_inputs()
    if source != fixture["source"]:
        raise ValueError("whole original provenance differs from capture fixture")
    for at, part in enumerate(parts):
        for key in (*ATTRIBUTES, "binding"):
            expected = fixture["geometry_arrays_sha256"][f"part{at}_{key}"]
            if hashlib.sha256(part[key].tobytes()).hexdigest() != expected:
                raise ValueError("whole original geometry/frame binding changed")
    points = np.concatenate([p["positions"] for p in parts]).astype(float)
    diagonal = float(np.linalg.norm(points.max(0) - points.min(0)))
    offsets = {
        "foreground": [np.zeros(3)],
        "ordered-background": [forward * diagonal],
        "ordered-union": [np.zeros(3), forward * diagonal],
        "interleaved-background": [right * (0.1 * diagonal)],
        "interleaved-union": [np.zeros(3), right * (0.1 * diagonal)],
    }
    output.mkdir(parents=True)
    arrays, runs, intervals, margins = {}, {}, {}, {}
    for name, translations in offsets.items():
        root = output / name
        moved = stage_mesh(root, parts, translations, query)
        projected = (moved - eye) @ basis.T
        margin = np.minimum(
            projected.min(0)
            - [-camera["x_mag_m"], -camera["y_mag_m"], camera["near_m"]],
            [camera["x_mag_m"], camera["y_mag_m"], camera["far_m"]] - projected.max(0),
        )
        if np.any(margin <= 0):
            raise ValueError("original triangles leave common capture: " + name)
        margins[name] = margin.tolist()
        intervals[name] = [float(projected[:, 2].min()), float(projected[:, 2].max())]
        runs[name] = native(args.binary.resolve(), root, "--material-fields")
        report = json.loads((root / "material-fields-result.json").read_text())[
            "cases"
        ][0]
        if report["capture"] != camera:
            raise ValueError("native capture changed exact camera/raster binding")
        width, height = camera["width_px"], camera["height_px"]
        colour = read(root / "base-colour.f32", (height, width, 4))
        depth = read(root / "base-depth.f32", (height, width))
        if not set(np.unique(colour[:, :, 3])) <= {0, 1} or not (depth > 0).any():
            raise ValueError(
                "registered point inputs need nonempty opaque native masks"
            )
        prefix = read(root / "base-integral.f32", (height + 1, width + 1, 4))
        if np.any(prefix[0]) or np.any(prefix[:, 0]):
            raise ValueError("exclusive integral border is not transparent zero")
        exact = np.pad(
            colour.astype(float).cumsum(0).cumsum(1), ((1, 0), (1, 0), (0, 0))
        )
        unit = np.finfo(np.float32).eps / 2
        gamma = (width + height) * unit / (1 - (width + height) * unit)
        if np.any(np.abs(prefix - exact) > gamma * exact):
            raise ValueError(
                "native positive prefix exceeds serial Float32 accumulation bound"
            )
        arrays.update(
            {
                name + "_colour": colour,
                name + "_depth": depth,
                name + "_integral": prefix,
            }
        )
        runs[name].update(
            gpu_ms=report["relight_filter_gpu_ms"],
            prefix_max_absolute_error=float(np.abs(prefix - exact).max()),
        )
    checks = compare(arrays, intervals)
    np.savez_compressed(output / "captures.npz", **arrays)
    result = {
        "camera": camera,
        "source": source,
        "source_fixture_sha256": digest(FIXTURE / "manifest.json"),
        "translations_m": {
            k: [v.tolist() for v in values] for k, values in offsets.items()
        },
        "forward_intervals_m": intervals,
        "minimum_vertex_clip_margins_m": margins,
        "checks": checks,
        "runs": runs,
        "payload_sha256": digest(output / "captures.npz"),
        "scope": "Native original point/union controls on identical rays, followed by existing GPU prefix. Fixed opaque source groups; unresolved source ordinals/archived opacity remain explicit. Vertex margins are finite controls, not general GPU-rounded metadata admission. F32 positive accumulation bound does not qualify local HDR rectangle differences (#1718), continuous coverage, external background, World/frame/pose or device budgets.",
    }
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "checks": checks,
                "payload_bytes": (output / "captures.npz").stat().st_size,
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
