"""Extract complete source profiles from native faces before constructing domains (#1189)."""

import argparse
import copy
import json
import subprocess
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from mvt_native_cut_profiles import canonical_profile
from mvt_stair_native_crosscuts import FIXTURE
from native_planar_mesh import ROOT, build_probe
from native_surface_ribbon import run as compare_ribbons


def write_input(path, cut):
    frame = [*cut["center_xy_m"], *cut["tangent_xy"], cut["half_span_m"]]
    with path.open("w") as stream:
        stream.write(
            str(len(cut["triangles"]))
            + " "
            + " ".join(format(v, ".17g") for v in frame)
            + "\n"
        )
        for triangle in cut["triangles"]:
            for point in triangle["enu_m"]:
                stream.write(" ".join(format(v, ".17g") for v in point) + "\n")


def transformed_cut(original, control):
    cut = copy.deepcopy(original)
    if control == "permuted":
        cut["triangles"].reverse()
        for triangle in cut["triangles"]:
            triangle["enu_m"].reverse()
    if control == "rigid":
        angle = 0.37
        matrix = np.array(
            [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
        )
        shift = [800, -300]
        points = {}

        def transform(point):
            key = tuple(point)
            if key not in points:
                points[key] = (np.asarray(point) @ matrix.T + shift).tolist()
            return points[key]

        cut["center_xy_m"] = transform(cut["center_xy_m"])
        cut["original_source_xy_m"] = [
            transform(p) for p in cut["original_source_xy_m"]
        ]
        cut["tangent_xy"] = (np.asarray(cut["tangent_xy"]) @ matrix.T).tolist()
        for triangle in cut["triangles"]:
            triangle["enu_m"] = [[*transform(p[:2]), p[2]] for p in triangle["enu_m"]]
    return cut


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    binary = build_probe(
        args.output,
        units=("SurfaceProfile.cpp",),
        probe_source=ROOT / "test/harness/spatial/SurfaceProfileProbe.cpp",
    )
    results, originals = [], []
    for index, original in enumerate(json.loads(FIXTURE.read_text())["cuts"]):
        for control in ("original", "permuted", "rigid"):
            cut = transformed_cut(original, control)
            expected = canonical_profile(cut)
            stem = f"cut-{index}-{control}"
            source, output = (
                args.output / (stem + suffix) for suffix in (".input", ".output")
            )
            write_input(source, cut)
            subprocess.run([str(binary), str(source), str(output)], check=True)
            rows = iter(output.read_text().splitlines())
            count, axis, faces, edges, scratch, storage = map(int, next(rows).split())
            points = np.array(
                [list(map(float, next(rows).split())) for _ in range(count)]
            )
            parents = [int(next(rows)) for _ in range(count - 1)]
            if next(rows, None) is not None:
                raise ValueError("trailing native profile output")
            np.testing.assert_allclose(
                points, expected["profile_xyz_m"], atol=1e-12, rtol=0
            )
            if (
                axis != expected["axis_vertex"]
                or faces != len(cut["triangles"])
                or edges != 3 * faces
            ):
                raise ValueError("native axis identity or input work differs")
            identities = {
                (t["page"], t["triangle"]): i for i, t in enumerate(cut["triangles"])
            }
            expected_parents = [
                identities[tuple(face)] for face in expected["native_faces"]
            ]
            if count == len(expected_parents) + 2:
                expected_parents.insert(axis, expected_parents[axis - 1])
            if parents != expected_parents:
                raise ValueError("a native profile interval lost its original cap face")
            results.append(
                {
                    "cut": index,
                    "control": control,
                    "points": count,
                    "axis_vertex": axis,
                    "face_visits": faces,
                    "edge_tests": edges,
                    "scratch_bytes": scratch,
                    "output_bytes": storage,
                }
            )
            if control == "original":
                expected["profile_xyz_m"] = points.tolist()
                expected["axis_vertex"] = axis
                expected["offsets_m"] = (
                    (points[:, :2] - expected["center_xy_m"])
                    @ np.array([-expected["tangent_xy"][1], expected["tangent_xy"][0]])
                ).tolist()
                originals.append(expected)
    (args.output / "native-profiles.json").write_text(
        json.dumps(originals, indent=2) + "\n"
    )
    with patch("native_surface_ribbon.cut_profiles", return_value=originals):
        compare_ribbons(SimpleNamespace(output=args.output / "ribbons"))
    result = {
        "numpy": np.__version__,
        "cases": results,
        "ribbon_cases": 6,
        "scope": "Native cap faces to complete native profiles and cropped source domains. Caller supplies a qualified source frame and candidate cap faces. Landing/section planning, height/body consumers and runtime/world integration remain separate.",
    }
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
