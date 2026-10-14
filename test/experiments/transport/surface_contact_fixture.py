"""Explicitly export the checked #69 native-reference fixture; default only verifies."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


ROOT = Path(__file__).resolve().parents[3]
GROUND = ROOT / "build/experiments/transport/ground"
FIXTURE = Path(__file__).resolve().parent / "fixtures/surface-sampling-vienna-4ea502363.npz"


def result_digest(points, normals, faces, reasons):
    return hashlib.sha256(points.tobytes() + normals.tobytes() + faces.tobytes()
                          + "\n".join(map(str, reasons)).encode()).hexdigest()


def generate(output):
    benchmark = json.loads((GROUND / "contacts-benchmark.json").read_text())
    source = GROUND / "Vienna-ground.npz"
    ground = json.loads(source.with_suffix(".json").read_text())
    if not benchmark["commit"].startswith("4ea502363") or not ground["commit"].startswith("4ea502363"):
        raise ValueError("fixture requires the checked #69 input and benchmark")
    reference = next(row for row in benchmark["methods"] if row["method"] == "predicate")
    with np.load(source) as data:
        mesh = Mesh(data["vertices"], data["faces"])
        road_vertex_map, road_count = data["road_vertex_map"], data["road_face_count"]
    random = np.random.default_rng(739)
    lo, hi = mesh.vertices[:, :2].min(axis=0), mesh.vertices[:, :2].max(axis=0)
    queries = np.vstack((random.uniform(lo - 10, hi + 10, (20000, 2)), mesh.vertices[:1000, :2],
                         mesh.vertices[mesh.triangles[:1000, :2], :2].mean(axis=1)))
    result = SurfaceSampler(mesh).sample(queries)
    query_sha = hashlib.sha256(queries.tobytes()).hexdigest()
    digest = result_digest(result.points_enu_m, result.normals_enu, result.face_indices, result.reasons)
    if query_sha != reference["input_sha256"] or digest != reference["result_sha256"]:
        raise ValueError("export differs from the exact checked 22000-query reference")
    output.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output, vertices=mesh.vertices, triangles=mesh.triangles, queries_xy_m=queries,
                        points_enu_m=result.points_enu_m, normals_enu=result.normals_enu, face_indices=result.face_indices,
                        reasons=np.asarray(result.reasons, dtype="U32"), road_vertex_map=road_vertex_map, road_face_count=road_count)
    metadata = dict(reference_commit=ground["commit"], frame=ground["frame"], queries=len(queries),
                    fixture_sha256=hashlib.sha256(output.read_bytes()).hexdigest(), query_sha256=query_sha,
                    result_sha256=digest, ground_npz_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    source_height_input_sha256=ground["source_height_input_sha256"], source_dem=ground["source_dem"],
                    source_usage=ground["source_usage"], xy_tolerance_m=1e-8, height_tolerance_m=1e-7,
                    native_height_tolerance_m=1e-7, native_normal_tolerance=1e-12,
                    scope="Explicitly generated derived mesh/contact reference, not network source or a native result. Face indices and reasons exact; numeric comparison tolerances explicit. Missing height/normal are NaN.")
    output.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")


def verify(path):
    meta = json.loads(path.with_suffix(".json").read_text())
    if hashlib.sha256(path.read_bytes()).hexdigest() != meta["fixture_sha256"]:
        raise ValueError("fixture byte pin changed")
    with np.load(path) as data:
        query = data["queries_xy_m"]
        digest = result_digest(data["points_enu_m"], data["normals_enu"], data["face_indices"], data["reasons"])
        if hashlib.sha256(query.tobytes()).hexdigest() != meta["query_sha256"] or digest != meta["result_sha256"]:
            raise ValueError("reference input/output pin changed")
        result = SurfaceSampler(Mesh(data["vertices"], data["triangles"])).sample(query)
        np.testing.assert_array_equal(result.face_indices, data["face_indices"])
        np.testing.assert_array_equal(np.asarray(result.reasons), data["reasons"])
        np.testing.assert_allclose(result.points_enu_m, data["points_enu_m"], rtol=0, atol=meta["native_height_tolerance_m"], equal_nan=True)
        np.testing.assert_allclose(result.normals_enu, data["normals_enu"], rtol=0, atol=meta["native_normal_tolerance"], equal_nan=True)
    return dict(reference_commit=meta["reference_commit"], queries=len(query), verified=True,
                bytes=path.stat().st_size, fixture_sha256=meta["fixture_sha256"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generate", action="store_true", help="explicit reference creation, never normal verification")
    parser.add_argument("--fixture", type=Path, default=FIXTURE)
    args = parser.parse_args()
    if args.generate:
        generate(args.fixture)
    print(json.dumps(verify(args.fixture)))


if __name__ == "__main__":
    main()
