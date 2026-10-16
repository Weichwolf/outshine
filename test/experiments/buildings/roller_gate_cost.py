"""Equal closed roller sheets: direct cap extrusion, generic Shape and strip envelope."""

import argparse
import gc
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from outshine_ref.buildings.modelling import Shape, heightfield_envelope
from outshine_ref.buildings.modelling.faces import Face
from outshine_ref.buildings.roller_gates import RollerGate, leaf_surface, pack, profile
from outshine_ref.buildings.section_roofs import RoofSection, section_surface
from structural_shell_geometry import native


def alternative(gate, opened, method):
    if method == "direct":
        return leaf_surface(gate, opened)
    section = profile(gate, opened)
    if method == "shape":
        ring = np.array([(section[0, 0], 0), (section[-1, 0], 0), *section[::-1]])
        vertices = np.c_[np.zeros(len(ring)), ring]
        raw = (
            Shape.from_faces(vertices, [Face(tuple(range(len(vertices))))])
            .extrude(gate.width_m)
            .emit()
        )
    elif method == "strips":
        top = section_surface(
            shapely.box(0, section[0, 0], gate.width_m, section[-1, 0]),
            RoofSection(tuple(map(tuple, section)), (0.0, 1.0)),
        )
        raw = heightfield_envelope(top, 0).mesh
    else:
        raise ValueError("Unknown equal-sheet construction")
    return pack(raw["vertices"][raw["faces"]], section, 10)


def physical(mesh):
    solid = native(mesh)
    p, uv = mesh["vertices"][mesh["faces"]], mesh["uv"][mesh["faces"]]
    for a, b in ((0, 1), (1, 2), (2, 0)):
        np.testing.assert_allclose(
            np.linalg.norm(p[:, a] - p[:, b], axis=1),
            np.linalg.norm(uv[:, a] - uv[:, b], axis=1),
            atol=1e-10,
        )
    return solid


def benchmark():
    output = []
    gate = RollerGate(3.8, 3)
    for opened in (0.0, 0.35, 0.5):
        reference = physical(leaf_surface(gate, opened))
        for method in ("direct", "shape", "strips"):
            mesh = alternative(gate, opened, method)
            candidate = physical(mesh)
            xor_volume = (candidate - reference).volume() + (
                reference - candidate
            ).volume()
            if (
                xor_volume > 1e-11
                or abs(candidate.surface_area() - reference.surface_area()) > 1e-9
            ):
                raise ValueError("Alternative did not retain the same physical sheet")
            for _ in range(2):
                alternative(gate, opened, method)
            times = []
            for _ in range(21):
                start = time.perf_counter()
                mesh = alternative(gate, opened, method)
                times.append((time.perf_counter() - start) * 1000)
            del mesh
            gc.collect()
            tracemalloc.start()
            mesh = alternative(gate, opened, method)
            current, peak = tracemalloc.get_traced_memory()
            live_blocks = sum(
                s.count for s in tracemalloc.take_snapshot().statistics("filename")
            )
            tracemalloc.stop()
            output.append(
                {
                    "method": method,
                    "opened": opened,
                    "runs_ms": times,
                    "p50_ms": float(np.median(times)),
                    "triangles": len(mesh["faces"]),
                    "vertices": len(mesh["vertices"]),
                    "output_arrays": len(mesh),
                    "output_bytes": sum(v.nbytes for v in mesh.values()),
                    "final_xyz_write_bytes": mesh["vertices"].nbytes,
                    "final_normal_uv_material_face_bytes": sum(
                        v.nbytes for k, v in mesh.items() if k != "vertices"
                    ),
                    "profile_points": len(profile(gate, opened)),
                    "traced_live_bytes": current,
                    "traced_peak_bytes": peak,
                    "traced_live_blocks": live_blocks,
                    "xor_volume_m3": xor_volume,
                    "volume_m3": candidate.volume(),
                    "surface_m2": candidate.surface_area(),
                }
            )
    return dict(
        revision(),
        cases=output,
        conditions="Normal process priority, 2 warmups and 21 timed repetitions; tracemalloc separate; OS load uncontrolled",
        scope="Same closed physical sheet and canonical developed metric UV/geometric normals; exact Native difference, surface and per-edge UV checks. Output writes/live traced allocations only; backend allocations, Python list writes and total copies remain unenumerated. No native/frame/device factor.",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = benchmark()
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    for row in result["cases"]:
        print(
            row["opened"],
            row["method"],
            row["triangles"],
            round(row["p50_ms"], 3),
            row["traced_peak_bytes"],
        )
