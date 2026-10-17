"""Equal profile surface: direct retained triangle cuts versus full logical split/emit."""

import argparse
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from broken_hip import surface
from evidence import revision
from outshine_ref.buildings.modelling import Part, Shape, heightfield_envelope
from provider_contract import attributes
from structural_shell_geometry import mesh_section
from tile_roof_envelopes import choose, roof_surface
from verify_roof_envelopes import measures
from verify_urban_roofs import control, sources


def logical(top, eave_m, rise_m):
    faces = top["faces"]
    shape = Shape.from_mesh(
        {
            "vertices": top["vertices"][faces].reshape(-1, 3),
            "faces": np.arange(faces.size).reshape(-1, 3),
            "uv": top["uv"][faces].reshape(-1, 2),
            "materials": top["materials"],
        }
    )
    pieces = shape.split("z", Part(rise_m * 0.15), Part(rise_m * 0.85), adjust=False)
    data, owners = [], []
    for piece in pieces:
        mesh = piece.emit()
        data.append(
            np.c_[
                mesh["vertices"][mesh["faces"]].reshape(-1, 3),
                mesh["uv"][mesh["faces"]].reshape(-1, 2),
            ]
        )
        owners.extend(np.asarray(piece.split_info.source_faces)[mesh["parts"]])
    data = np.concatenate(data)
    vertices = data[:, :3].copy()
    r = (vertices[:, 2] - eave_m) / rise_m
    vertices[:, 2] = eave_m + rise_m * np.where(
        r <= 0.15, r * 0.65 / 0.15, 0.65 + (r - 0.15) * 0.35 / 0.85
    )
    tolerance = max(1e-10, 64 * np.finfo(float).eps * max(abs(eave_m), rise_m, 1.0))
    for a, b in ((0, 0), (0.15, 0.65), (1, 1)):
        vertices[abs(data[:, 2] - (eave_m + rise_m * a)) <= tolerance, 2] = (
            eave_m + rise_m * b
        )
    triangles = vertices.reshape(-1, 3, 3)
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    output = {
        "vertices": vertices,
        "faces": np.arange(len(vertices), dtype=np.uint32).reshape(-1, 3),
        "uv": data[:, 3:5].copy(),
        "normals": np.repeat(normals, 3, axis=0),
    }
    for name in ("materials", "groups", "parts", "face_tag_mask"):
        if name in top:
            output[name] = top[name][owners].copy()
    return output


def equivalent(first, second, parameters, choice):
    products = [
        heightfield_envelope(m, parameters["minHeight"]) for m in (first, second)
    ]
    for product in products:
        measures(product)
    volume_error = abs(products[0].volume_m3 - products[1].volume_m3)
    if volume_error > 1e-7:
        raise ValueError("Profile alternatives disagree on occupied volume")
    section_error = 0.0
    for f in (0.1, 0.64, 0.66, 0.9):
        z = choice.eave_m + choice.rise_m * f
        a, b = [mesh_section(p.mesh, p.vertex_roots, z) for p in products]
        section_error = max(section_error, a.symmetric_difference(b).area)
    if section_error > 1e-7:
        raise ValueError("Profile alternatives disagree on actual sections")
    return {"volume_error_m3": volume_error, "maximum_section_error_m2": section_error}


def measure(method, top, choice, repeats):
    samples = []
    for i in range(repeats + 2):
        began = time.perf_counter()
        result = method(top, choice.eave_m, choice.rise_m)
        if i >= 2:
            samples.append((time.perf_counter() - began) * 1000)
    tracemalloc.start()
    result = method(top, choice.eave_m, choice.rise_m)
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return {
        "samples_ms": samples,
        "p50_ms": float(np.median(samples)),
        "p95_ms": float(np.percentile(samples, 95)),
        "traced_peak_bytes_separate_run": peak,
        "output_array_bytes": sum(a.nbytes for a in result.values()),
        "output_triangles": len(result["faces"]),
        "source_triangle_visits": 2 * len(top["faces"]),
    }


def run(output, repeats):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before profile work comparison")
    records = []
    for row in [control(shapely.box(0, 0, 20, 30)), *sources()]:
        parameters = attributes(row["properties"])
        choice = choose(row, parameters)
        top = roof_surface(row, choice)[0]
        direct = surface(top, choice.eave_m, choice.rise_m)
        record = {
            "id": row["id"],
            "input_triangles": len(top["faces"]),
            "direct": measure(surface, top, choice, repeats),
        }
        try:
            alternative = logical(top, choice.eave_m, choice.rise_m)
            record["equivalence"] = equivalent(direct, alternative, parameters, choice)
            record["logical"] = measure(logical, top, choice, repeats)
        except (ValueError, NotImplementedError, shapely.errors.GEOSException) as error:
            record["logical_rejected"] = str(error)
        records.append(record)
    output.write_text(
        json.dumps(
            proof
            | {
                "cases": records,
                "scope": "Same prepared input/profile requirements; surface work only, closure verified outside timings. Two warmups, normal-priority nonisolated host alongside nice10 renders. Tracemalloc separate from timing excludes native/GEOS allocations and cumulative/internal copies; output bytes are owned final arrays. Source visits count cutter scans; logical alternative additionally rebuilds logical faces/scopes. No device/frame/global optimality admission.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=21)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("Expected 1..21 measured repetitions")
    run(args.output, args.repeats)
