"""Read prepared local patches and compare with independent whole saved geometry."""

import argparse
import json
import resource
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_module_experiment import write_json
from stair_module_inputs import read_arrays
from stair_patch_experiment import compare
from stair_patch_inputs import load_prepared, query_points
from stair_support_audit import Reference
from stair_terrain_experiment import frozen
from wien_support_verify import unpack


def compare_queries(view, reference):
    points = query_points(view)
    totals = {"index": 0, "scan": 0}
    for cast in (False, True):
        mesh = (
            Mesh(reference.vertices.astype("f4"), reference.triangles)
            if cast
            else reference
        )
        sampler = SurfaceSampler(mesh)
        for point in points:
            bound = [*(point - 0.5), *(point + 0.5)]
            indexed = view.query(bound)
            scanned = view.query(bound, method="scan")
            if (
                indexed.face_keys != scanned.face_keys
                or indexed.reason != scanned.reason
            ):
                raise ValueError("indexed and scanned active local faces differ")
            for method, result in (("index", indexed), ("scan", scanned)):
                totals[method] += result.report["face_tests"]
            expected = sampler.sample([point])
            if indexed.mesh is None:
                if indexed.reason != "outside_surface" or expected.reasons != (
                    "outside_surface",
                ):
                    raise ValueError("local query omitted actual reference geometry")
                continue
            local = (
                Mesh(indexed.mesh.vertices.astype("f4"), indexed.mesh.triangles)
                if cast
                else indexed.mesh
            )
            actual = SurfaceSampler(local).sample([point])
            if (
                actual.reasons != expected.reasons
                or not np.array_equal(
                    actual.points_enu_m, expected.points_enu_m, equal_nan=True
                )
                or not np.array_equal(
                    actual.normals_enu, expected.normals_enu, equal_nan=True
                )
            ):
                raise ValueError(
                    "local whole-contact sample differs from global reference"
                )
    return {
        "points_per_precision": len(points),
        "double_float32_exact": True,
        "face_tests": totals,
    }


def pier_contacts(view, supports):
    report = json.loads((supports / "comparison.json").read_text())
    path = supports / (report["selected"] + ".npz")
    if digest(path) != report["methods"][report["selected"]]["mesh_sha256"]:
        raise ValueError("original whole support geometry changed")
    _products, contacts, _data = unpack(path)
    errors, maximum = [], 0
    for low, _high in contacts:
        xy = low.vertices[:, :2]
        result = view.query([*(xy.min(axis=0) - 0.001), *(xy.max(axis=0) + 0.001)])
        if result.mesh is None:
            raise ValueError("whole old pier has no complete prepared field")
        proof = Reference(result.mesh).check(low)
        errors.append(proof["float32_contact_error_m"])
        maximum = max(maximum, result.report["returned_faces"])
    return {
        "whole_piers": len(errors),
        "maximum_float32_error_m": max(errors, default=0),
        "maximum_local_faces": maximum,
    }


def verify(args):
    commit, began = frozen(), time.perf_counter()
    view, manifest = load_prepared(args.prepared)
    layout = read_arrays(args.reference / "world-layout.npz")
    field = read_arrays(args.reference / "world-field.npz")
    proof = compare(view, layout, field)
    reference = Mesh(np.c_[layout["xy"], field["heights"]], layout["faces"])
    proof.update(compare_queries(view, reference))
    proof["pier_contacts"] = pier_contacts(view, args.supports)
    result = {
        "generation_commit": manifest["commit"],
        "reader_commit": commit,
        "prepared_manifest_sha256": digest(args.prepared / "patches.json"),
        "reference_layout_sha256": digest(args.reference / "world-layout.npz"),
        "reference_field_sha256": digest(args.reference / "world-field.npz"),
        "proof": proof,
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "valid_exact_resident_surface": True,
        "scope": "Resident finite mesh/attribute/local contact equivalence only; not source-network growth, native cache, full routes or LOD.",
    }
    write_json(args.prepared / "verification.json", result)
    print(json.dumps(result), flush=True)
    return result


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--supports", type=Path, required=True)
    verify(parser.parse_args())
