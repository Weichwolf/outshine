"""Independent complete indexed-source reader of bounded regional draw parts."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_module_experiment import write_json


def inspect_part(part, source, owner):
    path = Path(part["mesh"])
    if digest(path) != part["mesh_sha256"]:
        raise ValueError("regional draw part bytes changed")
    with np.load(path, allow_pickle=False) as saved:
        data = dict(saved)
    positions, faces, vertices, ids = (
        data[k]
        for k in ("vertices", "triangles", "source_vertex_ids", "source_face_ids")
    )
    if (
        positions.dtype != np.dtype("f4")
        or not 1 <= len(positions) <= 65536
        or vertices.dtype.kind not in "iu"
        or ids.dtype.kind not in "iu"
        or len(np.unique(vertices)) != len(vertices)
        or vertices.shape != (len(positions),)
        or vertices.min() < 0
        or vertices.max() >= len(source["vertices"])
        or ids.min() < 0
        or ids.max() >= len(source["triangles"])
        or faces.dtype.kind not in "iu"
        or faces.shape != (len(ids), 3)
        or faces.min() < 0
        or faces.max() >= len(vertices)
        or not np.isfinite(positions).all()
    ):
        raise ValueError("invalid bounded regional/source identity streams")
    if not np.array_equal(vertices[faces], source["triangles"][ids]):
        raise ValueError("regional split changed actual original face topology")
    for key in ("materials", "face_base_colour_linear_rgba"):
        if not np.array_equal(data[key], source[key][ids]):
            raise ValueError("regional split changed source material ownership")
    if not np.array_equal(
        data["normals"].reshape(-1, 3, 3), source["normals"].reshape(-1, 3, 3)[ids]
    ):
        raise ValueError("regional split changed source corner normals")
    expected = source["vertices"][vertices] - np.asarray(owner["origin_m"])
    error = np.abs(positions.astype(float) - expected)
    half_ulp = np.spacing(np.abs(positions)).astype(float) / 2
    if np.any(error > half_ulp):
        raise ValueError("regional payload is not a nearest Float32 source position")
    actual = positions.astype(float) + np.asarray(owner["origin_m"])
    if part["preserved_legacy_draw"]:
        if not np.array_equal(
            actual, source["vertices"][vertices].astype("f4").astype(float)
        ):
            raise ValueError("preserved legacy draw positions changed")
    else:
        reference = source["vertices"][source["triangles"][ids]]
        triangles = actual[faces]
        old_cross = np.cross(
            reference[:, 1] - reference[:, 0], reference[:, 2] - reference[:, 0]
        )
        new_cross = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        if np.any(np.einsum("ij,ij->i", old_cross, new_cross) <= 0):
            raise ValueError("new regional draw has a nonpositive original face")
    return ids, vertices, positions, float(np.linalg.norm(error, axis=1).max())


def verify(root, output):
    if output.exists():
        raise ValueError("fresh independent receipt required")
    report = json.loads((root / "comparison.json").read_text())
    if (
        digest(report["original_scene"]) != report["original_scene_sha256"]
        or digest(report["frame_input"]) != report["frame_input_sha256"]
    ):
        raise ValueError("original scene/frame input changed")
    original = json.loads(Path(report["original_scene"]).read_text())
    frames = json.loads(Path(report["frame_input"]).read_text())
    source_owners = {b["id"]: b for b in original.get("bodies", [original])}
    if set(source_owners) != {o["id"] for o in report["bodies"]} or len(
        source_owners
    ) != len(report["bodies"]):
        raise ValueError("regional scene lost or duplicated a complete source owner")
    if set(frames) != set(source_owners):
        raise ValueError("source-owned frames are incomplete")
    scene = json.loads((root / "regional-scene.json").read_text())
    draws = {b["id"]: b for b in scene["bodies"]}
    if not 1 <= len(draws) <= 64 or len(draws) != len(scene["bodies"]):
        raise ValueError("complete bounded distinct regional objects required")
    results, seen = [], set()
    for owner in report["bodies"]:
        original_body = source_owners[owner["id"]]
        frame = frames[owner["id"]]
        if (
            owner["original_mesh"] != original_body["mesh"]
            or owner["original_mesh_sha256"] != original_body["mesh_sha256"]
        ):
            raise ValueError("regional owner changed its original source")
        if digest(owner["original_mesh"]) != owner["original_mesh_sha256"]:
            raise ValueError("original full source changed")
        with np.load(owner["original_mesh"], allow_pickle=False) as saved:
            source = dict(saved)
        all_ids, all_vertices, maximum = [], [], 0.0
        cache = np.empty(source["vertices"].shape, dtype="f4")
        used = np.zeros(len(cache), dtype=bool)
        for part in owner["draw_parts"]:
            name = part["draw_part"]
            if (
                name in seen
                or name not in draws
                or part["mesh_sha256"] != draws[name]["mesh_sha256"]
                or original_body["colours"] != draws[name]["colours"]
                or frame["origin_m"] != draws[name]["origin_m"]
                or frame["preserve_legacy_draw"] is not part["preserved_legacy_draw"]
            ):
                raise ValueError("ambiguous or missing original draw owner")
            ids, vertices, positions, error = inspect_part(part, source, draws[name])
            shared = used[vertices]
            if not np.array_equal(cache[vertices[shared]], positions[shared]):
                raise ValueError("shared original vertex changed at a draw-part seam")
            cache[vertices] = positions
            used[vertices] = True
            seen.add(name)
            all_ids.append(ids)
            all_vertices.append(vertices)
            maximum = max(maximum, error)
        if not np.array_equal(
            np.concatenate(all_ids), np.arange(len(source["triangles"]))
        ):
            raise ValueError(
                "regional draw omitted/reordered/duplicated original faces"
            )
        results.append(
            {
                "id": owner["id"],
                "faces": len(source["triangles"]),
                "referenced_source_vertices": len(
                    np.unique(np.concatenate(all_vertices))
                ),
                "maximum_local_rounding_error_m": maximum,
                "parts": len(all_ids),
            }
        )
    if seen != set(draws):
        raise ValueError("unexpected source-free regional object")
    result = {
        "source_owners": len(results),
        "draw_objects": len(draws),
        "owners": results,
        "source_report_sha256": digest(root / "comparison.json"),
        "verifier_sha256": digest(__file__),
        "scope": "Complete source face/vertex provenance, unchanged corner normals/materials and bounded nearest Float32 encoding. No physical or native/device admission.",
    }
    write_json(output, result)
    print(
        {
            "owners": len(results),
            "objects": len(draws),
            "faces": sum(r["faces"] for r in results),
        },
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    verify(args.products, args.output)
