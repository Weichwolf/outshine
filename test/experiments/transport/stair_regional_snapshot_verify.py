"""Read saved position streams, independently check whole facets and neighbor bindings."""

from fractions import Fraction

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_module_experiment import write_json
from stair_regional_snapshot import arguments, original
from stair_terrain_experiment import frozen


def decoded(data, count):
    if set(data) != {"positions", "origins", "origin_ids"}:
        raise ValueError("complete position stream artifact required")
    q, origins, ids = (data[k] for k in ("positions", "origins", "origin_ids"))
    if (
        q.shape != (count, 2)
        or q.dtype != np.dtype("f4")
        or origins.ndim != 2
        or origins.shape[1] != 2
        or not 1 <= len(origins) <= 33
        or origins.dtype != np.dtype("f8")
        or ids.dtype != np.dtype("u1")
        or ids.shape != ((count,) if len(origins) > 1 else (0,))
        or (ids.size and ids.max() >= len(origins))
        or not np.isfinite(q).all()
        or not np.isfinite(origins).all()
    ):
        raise ValueError("invalid independent position stream representation")
    result = q.astype(float)
    for index, origin in enumerate(origins):
        if ids.size:
            result[ids == index] += origin
        else:
            result += origin
    return result


def whole_case(data, points, faces, anchors, origin, peer_origin):
    actual = decoded(data, len(points))
    peer = (points[anchors] - peer_origin).astype("f4").astype(float) + peer_origin
    if not np.array_equal(actual[anchors], peer):
        raise ValueError("actual neighboring boundary positions changed")
    expected = (points - origin).astype("f4").astype(float) + origin
    expected[anchors] = peer
    if not np.array_equal(actual, expected):
        raise ValueError(
            "actual region interior differs from original encoded positions"
        )
    owners = (
        data["origin_ids"]
        if data["origin_ids"].size
        else np.zeros(len(points), dtype="u1")
    )
    origins = data["origins"][owners]
    interior = np.ones(len(points), bool)
    interior[anchors] = False
    if not np.all(origins[interior] == origin) or not np.all(
        np.all(origins[anchors] == origin, axis=1)
        | np.all(origins[anchors] == peer_origin, axis=1)
    ):
        raise ValueError(
            "encoded origin is not bound to the original region or neighbor"
        )
    fractions = [(Fraction(float(x)), Fraction(float(y))) for x, y in actual]
    minimum = None
    for a, b, c in faces:
        pa, pb, pc = (fractions[int(i)] for i in (a, b, c))
        signed = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pb[1] - pa[1]) * (pc[0] - pa[0])
        if signed <= 0:
            raise ValueError("encoded whole terrain facet is not exactly positive")
        minimum = signed if minimum is None else min(minimum, signed)
    if len(np.unique(actual, axis=0)) != len(points) or not shapely.coverage_is_valid(
        shapely.polygons(actual[faces])
    ):
        raise ValueError("encoded terrain lost conforming coverage or unique vertices")
    return actual, {
        "exact_positive_faces": len(faces),
        "exact_minimum_signed_double_area_m2": float(minimum),
        "exact_neighbor_vertices": len(anchors),
        "maximum_vertex_displacement_m": float(
            np.linalg.norm(actual - points, axis=1).max()
        ),
    }


def verify(args):
    import json

    report = json.loads((args.output / "comparison.json").read_text())
    points, faces, anchors, origin, peers = original(args.input)
    names = {
        peer + "-" + method + ".npz"
        for peer in peers
        for method in ("copy", "rebase_if_exact")
    }
    if (
        report["input_sha256"] != digest(args.input)
        or (report["vertices"], report["faces"], report["boundary_vertices"])
        != (len(points), len(faces), len(anchors))
        or set(report["artifacts"]) != names
        or {c["name"] for c in report["cases"]} != names
        or len(report["cases"]) != len(names)
    ):
        raise ValueError("incomplete original input or region trial inventory")
    proofs, equality = {}, {}
    for case in report["cases"]:
        if (
            case["peer"] not in peers
            or case["method"] not in ("copy", "rebase_if_exact")
            or case["name"] != case["peer"] + "-" + case["method"] + ".npz"
        ):
            raise ValueError("saved source/method binding differs from its artifact")
        path = args.output / case["name"]
        if digest(path) != report["artifacts"][case["name"]]:
            raise ValueError("position stream artifact changed")
        with np.load(path, allow_pickle=False) as archive:
            data = dict(archive)
        sizes = {
            "origin_count": len(data["origins"]),
            "position_bytes": data["positions"].nbytes,
            "origin_bytes": data["origins"].nbytes,
            "origin_id_bytes": data["origin_ids"].nbytes,
        }
        if any(case[key] != value for key, value in sizes.items()):
            raise ValueError("saved position stream costs differ from actual arrays")
        actual, proofs[case["name"]] = whole_case(
            data, points, faces, anchors, origin, peers[case["peer"]]
        )
        if case["method"] == "copy":
            peer_origin = peers[case["peer"]]
            expected = (points[anchors] - peer_origin).astype("f4")
            owner_ids = (
                data["origin_ids"]
                if data["origin_ids"].size
                else np.zeros(len(points), dtype="u1")
            )
            if not np.all(
                data["origins"][owner_ids[anchors]] == peer_origin
            ) or not np.array_equal(
                data["positions"][anchors].view("u4"), expected.view("u4")
            ):
                raise ValueError(
                    "neighbor origin or original Float32 bits were not copied"
                )
        previous = equality.setdefault(case["peer"], actual)
        if not np.array_equal(actual, previous):
            raise ValueError("encoding alternatives have different actual geometry")
    proof = {
        "reader_commit": frozen(),
        "products_report_sha256": digest(args.output / "comparison.json"),
        "whole_cases": proofs,
        "same_actual_geometry": True,
    }
    if (args.output / "verification.json").exists():
        raise ValueError("fresh reader receipt required")
    write_json(args.output / "verification.json", proof)
    return proof


if __name__ == "__main__":
    proof = verify(arguments())
    print({"whole_cases": len(proof["whole_cases"]), "same_actual_geometry": True})
