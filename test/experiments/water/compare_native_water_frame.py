"""Original Ill geography -> canonical frame/CSR and physical facet controls (#760)."""

import argparse
import hashlib
import json
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from native_river_inputs import read_v2, surface_rings
from outshine_ref.water.geodetic import local_points, query_facet
from pyproj import Geod, Transformer
from pyproj.enums import TransformDirection
from river_membership import face_membership
from river_surface import surface_mesh
from shapely.geometry import Polygon


def original_case(manifest_path):
    manifest = json.loads(manifest_path.read_text())
    root = manifest_path.parent
    original = manifest["historical_v2"]
    data = read_v2(root / original["file"], original["sha256"])
    if data["source_bytes"] != original["bytes"]:
        raise ValueError("historical byte count does not match manifest")
    observation = manifest["observed_v3"]
    path = root / observation["file"]
    if path.stat().st_size != observation["bytes"] or observation["bytes"] > 1 << 20:
        raise ValueError("later observation byte budget/count mismatch")
    blob = path.read_bytes()
    if (
        len(blob) != observation["bytes"]
        or hashlib.sha256(blob).hexdigest() != observation["sha256"]
    ):
        raise ValueError("later observation digest mismatch")
    legacy = json.loads((root / manifest["fixture"]).read_text())
    if (
        manifest["coordinate_order"] != ["latitude_deg", "longitude_deg"]
        or manifest["original_selection"] != legacy["selection"]
    ):
        raise ValueError("historical input coordinate/selection contract mismatch")
    return data, manifest, legacy


def footprints(data, selection, anchor):
    zero, elevated, original_geo, parents, polygons, triangles, face_parents = (
        [],
        [],
        [],
        [],
        [],
        [],
        [],
    )
    for index in selection:
        level = float(data["surfaces"][index]["level_m"])
        rings = [r[:, ::-1] for r in surface_rings(data, index)]
        at_zero = [local_points(anchor, r, 0) for r in rings]
        at_level = [local_points(anchor, r, level) for r in rings]
        polygons.append(Polygon(at_zero[0][:, :2], [r[:, :2] for r in at_zero[1:]]))
        polygon = Polygon(at_level[0][:, :2], [r[:, :2] for r in at_level[1:]])
        if not polygon.is_valid or not polygons[-1].is_valid:
            raise ValueError("selected original projected footprint invalid")
        lookup = {tuple(v[:2]): v for v in np.concatenate(at_level)}
        for tri in shapely.get_parts(shapely.constrained_delaunay_triangles(polygon)):
            triangles.append(
                [lookup[tuple(q)] for q in np.asarray(tri.exterior.coords)[:3]]
            )
            face_parents.append(index)
        zero.extend(np.concatenate(at_zero))
        elevated.extend(np.concatenate(at_level))
        original_geo.extend(np.concatenate(rings))
        parents.extend([index] * sum(map(len, rings)))
    return {
        "original_ring_lon_lat": np.array(original_geo),
        "ring_zero_enu_m": np.array(zero),
        "ring_model_enu_m": np.array(elevated),
        "ring_surface_index": np.array(parents, dtype="<u4"),
        "fragment_triangles_enu_m": np.array(triangles),
        "fragment_surface_index": np.array(face_parents, dtype="<u4"),
    }, polygons


def physical_facets(arrays, data, anchor):
    triangles = arrays["fragment_triangles_enu_m"]
    centre = triangles.mean(axis=1)
    inverse = Transformer.from_pipeline(
        f"+proj=pipeline +step +proj=cart +ellps=WGS84 +step +proj=topocentric +ellps=WGS84 +lon_0={anchor[0]} +lat_0={anchor[1]} +h_0=0"
    )
    longitude, latitude, height = inverse.transform(
        *centre.T, direction=TransformDirection.INVERSE
    )
    geographic = np.c_[longitude, latitude]
    values = [
        query_facet(anchor, tri, [point], metric_height_m=0)
        for tri, point in zip(triangles, geographic, strict=True)
    ]
    actual = np.array([v["plane_height_m"][0] for v in values])
    grade = np.array([v["grade_m_per_m"][0] for v in values])
    enu_grade = np.array([v["enu_up_grade"] for v in values])
    if not all(v["inside_numeric"][0] for v in values):
        raise ValueError("physical-centroid query lost its triangle")
    np.testing.assert_allclose(actual, height, rtol=0, atol=64 * np.spacing(6378137.0))
    levels = data["surfaces"][arrays["fragment_surface_index"]]["level_m"].astype(float)
    arrays.update(
        facet_query_lon_lat=geographic,
        facet_plane_height_m=actual,
        facet_proj_height_m=np.asarray(height),
        facet_grade_m_per_m=grade,
        facet_enu_up_grade=enu_grade,
    )
    return {
        "faces": len(triangles),
        "queries_inside": len(values),
        "independent_proj_inverse_height_error_max_m": float(
            abs(actual - height).max()
        ),
        "sampled_fragment_level_error_max_m": float(abs(actual - levels).max()),
        "sampled_grade_vs_enu_up_max_m_per_m": float(abs(grade - enu_grade).max()),
        "scope": "Per-fragment Python triangulation/centroid checks, not native topology parity or a whole-face grade proof.",
    }


def course_controls(data, selection, anchor, scale):
    geographic, levels, addresses = [], [], []
    for index in selection:
        c = data["courses"][index]
        p = data["points_lat_lon"][
            c["first_point"] : c["first_point"] + c["point_count"], ::-1
        ]
        h = data["levels"][c["first_level"] : c["first_level"] + c["point_count"]]
        geographic.extend(np.stack((p[:-1], p[1:]), axis=1))
        levels.extend(np.c_[h[:-1], h[1:]])
        addresses.extend((index, i) for i in range(len(p) - 1))
    geographic, levels = np.array(geographic), np.array(levels)
    placed = local_points(anchor, geographic.reshape(-1, 2), levels.ravel()).reshape(
        -1, 2, 3
    )
    planar = (geographic - anchor) * scale
    _, _, distance = Geod(ellps="WGS84").inv(
        geographic[:, 0, 0],
        geographic[:, 0, 1],
        geographic[:, 1, 0],
        geographic[:, 1, 1],
    )
    distance = np.asarray(distance)
    valid = distance > 0
    delta = levels[:, 1].astype(float) - levels[:, 0]
    model_grade = np.divide(
        delta, distance, out=np.full(len(distance), np.nan), where=valid
    )
    xy_distance = np.linalg.norm(placed[:, 1, :2] - placed[:, 0, :2], axis=1)
    enu_grade = np.divide(
        placed[:, 1, 2] - placed[:, 0, 2],
        xy_distance,
        out=np.full(len(distance), np.nan),
        where=valid & (xy_distance > 0),
    )
    legacy_distance = np.linalg.norm(planar[:, 1] - planar[:, 0], axis=1)
    arrays = {
        "course_original_address": np.array(addresses, dtype="<u4"),
        "course_original_lon_lat": geographic,
        "course_original_levels_m": levels,
        "course_geodesic_length_m": distance,
        "course_model_height_grade": model_grade,
        "course_enu_up_grade": enu_grade,
    }
    return arrays, {
        "original_courses": len(selection),
        "original_segments": len(distance),
        "measured_segments": int(valid.sum()),
        "omitted_segments": [
            {
                "course": addresses[i][0],
                "segment": addresses[i][1],
                "reason": "zero_geodesic_length",
            }
            for i in np.flatnonzero(~valid)
        ],
        "geodesic_vs_legacy_length_difference_max_m": float(
            abs(distance[valid] - legacy_distance[valid]).max()
        ),
        "endpoint_model_grade_vs_enu_up_max_m_per_m": float(
            abs(model_grade[valid] - enu_grade[valid]).max()
        ),
        "scope": "Same original endpoint order; declared height delta/geodesic distance versus ENU-Up delta/anchor-XY distance, not observed flow tags or a solver admission.",
    }


def compare(data, manifest, legacy, old):
    anchor = np.array(legacy["frame"]["origin_lon_lat"])
    selection = manifest["original_selection"]
    arrays, polygons = footprints(data, selection["selected_surface_indices"], anchor)
    np.testing.assert_array_equal(
        old["surface_source_indices"], selection["selected_surface_indices"]
    )
    delta = arrays["original_ring_lon_lat"] - anchor
    scale = (delta * old["ring_points"]).sum(0) / (delta * delta).sum(0)
    residual = float(abs(delta * scale - old["ring_points"]).max())
    if residual > 1e-8:
        raise ValueError(
            "legacy planar reference is not aligned with original ring order"
        )
    xy_difference = arrays["ring_zero_enu_m"][:, :2] - old["ring_points"]
    levels = data["surfaces"][arrays["ring_surface_index"]]["level_m"].astype(float)
    up_difference = arrays["ring_model_enu_m"][:, 2] - levels
    arrays.update(
        legacy_xy=old["ring_points"].copy(),
        ring_enu_up_minus_model_height_m=up_difference,
    )
    mesh, coverage = surface_mesh(shapely.union_all(polygons), 20)
    csr = face_membership(mesh.vertices, mesh.triangles, polygons)
    if np.any(csr.status) or csr.uncovered_area_m2.sum() > max(
        1e-8, coverage["area_m2"] * 1e-12
    ):
        raise ValueError("canonical reference provenance incomplete")
    arrays.update(
        canonical_planar_xy=mesh.vertices[:, :2],
        canonical_triangles=mesh.triangles,
        source_offsets=csr.offsets,
        source_members=csr.members,
        source_representative=csr.representative,
        source_original_indices=np.array(
            selection["selected_surface_indices"], dtype="<u4"
        ),
    )
    courses, course_report = course_controls(
        data, selection["selected_course_indices"], anchor, scale
    )
    arrays.update(courses)
    overlaps = []
    for i, left in enumerate(polygons):
        for k in range(i + 1, len(polygons)):
            area = left.intersection(polygons[k]).area
            if area > 0:
                first, second = (
                    selection["selected_surface_indices"][i],
                    selection["selected_surface_indices"][k],
                )
                overlaps.append(
                    {
                        "original_surfaces": [first, second],
                        "overlap_area_m2": float(area),
                        "original_level_difference_m": abs(
                            float(data["surfaces"][first]["level_m"])
                            - float(data["surfaces"][second]["level_m"])
                        ),
                    }
                )
    report = {
        "anchor_lon_lat": anchor.tolist(),
        "legacy_scale_measured_m_per_degree": scale.tolist(),
        "legacy_ring_alignment_max_m": residual,
        "legacy_vs_wgs84_zero_xy_max_m": float(
            np.linalg.norm(xy_difference, axis=1).max()
        ),
        "enu_up_minus_model_height_range_m": [
            float(up_difference.min()),
            float(up_difference.max()),
        ],
        "height_induced_xy_shift_max_m": float(
            np.linalg.norm(
                arrays["ring_model_enu_m"][:, :2] - arrays["ring_zero_enu_m"][:, :2],
                axis=1,
            ).max()
        ),
        "canonical_reference_coverage": coverage,
        "canonical_reference_membership": csr.work,
        "canonical_member_uncovered_sum_m2": float(csr.uncovered_area_m2.sum()),
        "canonical_member_reference_counts": np.bincount(
            csr.members, minlength=len(polygons)
        ).tolist(),
        "fragment_facets": physical_facets(arrays, data, anchor),
        "courses": course_report,
        "overlapping_original_prior_levels": overlaps,
    }
    report["outside_explicit_selection"] = {
        "original_surfaces": [
            i
            for i in range(len(data["surfaces"]))
            if i not in selection["selected_surface_indices"]
        ],
        "original_courses": [
            i
            for i in range(len(data["courses"]))
            if i not in selection["selected_course_indices"]
        ],
        "reason": "outside the preserved finite fixture selection; originals retained in WATER2",
    }
    return arrays, report


def plot(arrays, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 2, figsize=(12.8, 6.5), layout="constrained")
    p = arrays["ring_zero_enu_m"]
    image = axes[0].scatter(
        p[:, 0],
        p[:, 1],
        c=arrays["ring_enu_up_minus_model_height_m"],
        s=8,
        cmap="viridis",
    )
    fig.colorbar(
        image, ax=axes[0], label="ENU-Up − same numerical geodetic model height [m]"
    )
    axes[0].set(
        xlabel="Canonical East [m]",
        ylabel="Canonical North [m]",
        title="485 original ring points; no geographic reconstruction",
    )
    axes[0].set_aspect("equal")
    axes[1].plot(
        arrays["course_model_height_grade"],
        label="Δmodel geodetic height / geodesic length",
    )
    axes[1].plot(
        arrays["course_enu_up_grade"], label="ΔENU-Up / anchor-XY length", alpha=0.75
    )
    axes[1].set(
        xlabel="Original course/segment order (121 records)",
        ylabel="Endpoint grade [m/m]",
        title="Different height/distance definitions, same source endpoints",
    )
    axes[1].legend(fontsize=8)
    axes[1].grid(alpha=0.25)
    fig.suptitle(
        "Original Ill inputs; ASL numbers interpreted only as the declared existing model, no geoid conversion"
    )
    fig.savefig(output, dpi=120)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or out.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp outside the checkout")
    out.mkdir(parents=True, exist_ok=False)
    tracemalloc.start()
    began = time.perf_counter()
    data, manifest, legacy = original_case(args.input)
    old_path = args.input.parent / Path(manifest["fixture"]).with_suffix(".npz")
    legacy_digest = hashlib.sha256(old_path.read_bytes()).hexdigest()
    if legacy_digest != legacy["input_sha256"]:
        raise ValueError("legacy planar control digest mismatch")
    with np.load(old_path, allow_pickle=False) as old:
        arrays, report = compare(data, manifest, legacy, old)
    numeric_s = time.perf_counter() - began
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    np.savez(out / "original-frame.npz", **arrays)
    (out / "source-observation.json").write_text(
        json.dumps(legacy["source_semantics_observation"], indent=2) + "\n"
    )
    began = time.perf_counter()
    plot(arrays, out / "original-frame.png")
    plot_s = time.perf_counter() - began
    report.update(
        scope="Original WATER2 geometry/values; W3 metadata is a later observation. Same declared numerical height interpretation as current native placement; no observed geoid conversion. Canonical planar topology has no fitted height; physical fragment triangulations are Python controls, no native gate.",
        source_bytes=data["source_bytes"],
        consumed_bytes=data["consumed_bytes"],
        native_manifest_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
        legacy_manifest_sha256=hashlib.sha256(
            (args.input.parent / manifest["fixture"]).read_bytes()
        ).hexdigest(),
        legacy_npz_sha256=legacy_digest,
        native_inputs=manifest,
        output_array_bytes=sum(a.nbytes for a in arrays.values()),
        numeric_wall_s_instrumented=numeric_s,
        plot_wall_s=plot_s,
        python_traced_numeric_peak_bytes=peak,
        memory_scope="Trace includes Python/NumPy/source buffers but excludes GEOS/PROJ native allocations. No native/frame/memory budget admission.",
        artifacts_sha256={
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(out.iterdir())
        },
    )
    (out / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "source_bytes",
                    "legacy_vs_wgs84_zero_xy_max_m",
                    "enu_up_minus_model_height_range_m",
                    "fragment_facets",
                    "output_array_bytes",
                )
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
