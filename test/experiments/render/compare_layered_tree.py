"""Shared field-tree demand, storage and full unchanged-camera geometry/attribute comparison."""

import argparse
import gzip
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from attributed_prisms import native_fields, native_inputs
from compare_attributed_prisms import diagnostic_poses, field_errors, picture
from compare_plane_cloud import image_roi, select_full_poses
from full_view_reference import load, views
from layered_tree_bake import cook
from layered_tree_plan import Plan, select
from layered_tree_render import load_cut, load_header, query
from native_capture_meshes import independent_depth
from primitive_bvh import build
from triangle_image_reference import (
    average_samples,
    depth_difference,
    difference,
    image_rays,
    intersect_source,
    prepare_source,
    relight,
)


def global_metadata(path, document, observer, profile):
    with gzip.open(path.parent / "inventory.json.gz", "rt") as stream:
        inventory = json.load(stream)
    rows = inventory["rows"]
    anchor = np.array(document["anchor_ecef"])
    bounds = np.array([r["bounds_ecef"] for r in rows]).reshape(-1, 2, 3) - anchor
    hierarchy = build(bounds)
    children = hierarchy.children.copy()
    leaf = children[:, 0] < 0
    children[leaf] = -1
    tree = Plan(hierarchy.bounds, children, 8)
    chosen, unresolved, tests = select(tree, observer, profile)
    leaves = []
    for node in np.r_[chosen, unresolved]:
        pending = [int(node)]
        while pending:
            at = pending.pop()
            a, b = hierarchy.children[at]
            if a < 0:
                leaves.append(int(b))
            else:
                pending.extend((int(a), int(b)))
    np.testing.assert_array_equal(np.sort(leaves), np.arange(len(rows)))
    selected = next(
        r
        for r in rows
        if r["parent"] == document["parent"] and r["key"] == document["view_key"]
    )
    return {
        "input_views": len(rows),
        "metadata_nodes": len(children),
        "selected_joint_nodes": len(chosen),
        "needs_finer_metadata_or_fields": len(unresolved),
        "node_tests": tests,
        "header_bytes": hierarchy.bounds.nbytes + hierarchy.children.nbytes,
        "selected_view_declared_extent_m": np.ptp(
            np.array(selected["bounds_ecef"]).reshape(2, 3), axis=0
        ).tolist(),
        "geometry_or_field_payloads_loaded": 0,
        "scope": "metadata-only partition; no global field products, image proof or residency gain",
    }


def one_view(source, prepared, inputs, cut, view, suns, output, *, query_fn=query):
    extent = {
        "positions": np.concatenate((source["positions"], cut["source"]["positions"]))
    }
    box = image_roi(extent, view)
    if box is None:
        return {"offscreen": True}
    width, height = (box[1] - box[0]).astype(int)
    if width * height * 4 > 4194304:
        raise ValueError("finite same-profile ROI budget exceeded")
    origin, direction = image_rays(view, box, width, height, 2)
    depth, face = intersect_source(source, origin, direction, prepared=prepared)
    fields = native_fields(inputs, origin, direction, depth, face)
    start = time.perf_counter()
    candidate, values, owners, tag = query_fn(cut, origin, direction)
    query_s = time.perf_counter() - start
    a, b = np.isfinite(depth), np.isfinite(candidate)
    common = a & b
    source_id = np.full(len(face), -1, np.int32)
    source_id[a] = source["binding"][face[a], 1]
    controls = np.unique(
        np.r_[
            np.linspace(0, len(a) - 1, min(8, len(a))).astype(int),
            np.flatnonzero(a)[:: max(1, int(a.sum()) // 8)][:8],
        ]
    )
    brute = independent_depth(
        source["positions"][source["indices"]], origin[controls], direction[controls]
    )
    np.testing.assert_array_equal(np.isfinite(brute), a[controls])
    finite = np.isfinite(brute)
    np.testing.assert_allclose(
        brute[finite], depth[controls][finite], atol=1e-8, rtol=1e-10
    )
    lighting = []
    for sun in suns:
        images = []
        for attrs, d in ((fields, depth), (values, candidate)):
            rgb = relight(attrs | {"colour": attrs["colour"][:, :3]}, sun, d)
            images.append(
                (
                    average_samples(rgb, width, height, 2),
                    average_samples(np.isfinite(d).astype(float), width, height, 2),
                )
            )
        lighting.append(difference(np.dstack(images[0]), np.dstack(images[1])))
    result = {
        "roi": box.tolist(),
        "width": int(width),
        "height": int(height),
        "subsamples": len(depth),
        "original_hits": int(a.sum()),
        "candidate_hits": int(b.sum()),
        "lost_samples": int((a & ~b).sum()),
        "added_samples": int((~a & b).sum()),
        "fields": field_errors(fields, values, common, source_id == owners),
        "depth": depth_difference(depth, candidate, np.ones(len(depth), bool)),
        "lighting": lighting,
        "query_wall_s": query_s,
        "independent_native_rays": len(controls),
    }
    raw = {
        "original_depth": depth,
        "candidate_depth": candidate,
        "original_face": face,
        "original_source": source_id,
        "candidate_source": owners,
        "candidate_original_face_tag": tag,
        "width": width,
        "height": height,
    }
    raw.update({"original_" + k: v for k, v in fields.items()})
    raw.update({"candidate_" + k: v for k, v in values.items()})
    np.savez_compressed(output, **raw)
    return result


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    temp = Path(tempfile.gettempdir()).resolve()
    repository = Path(__file__).resolve().parents[3]
    if any(
        not p.resolve().is_relative_to(temp) or p.resolve().is_relative_to(repository)
        for p in (args.product, args.output)
    ):
        parser.error("private product/report paths belong in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    source, document, context, receipt = load(args.input)
    reference_load = time.perf_counter() - start
    poses = views(document, context)
    eyes = np.array([v[0] for v in poses.values()])
    observer = np.stack((eyes.min(0), eyes.max(0)))
    first = next(iter(poses.values()))
    profile = {"focal_px": first[2], "width": int(first[3]), "height": int(first[4])}
    assembly = None
    provenance = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "parent": document["parent"],
        "view": document["view_key"],
    }
    if not args.product.exists():
        start = time.perf_counter()
        cook(source, observer, profile, args.product, provenance=provenance)
        assembly = time.perf_counter() - start
    if json.loads((args.product / "cook.json").read_text())["provenance"] != provenance:
        raise ValueError("field product belongs to a different original input")
    tree, header = load_header(args.product)
    nodes, unresolved, tests = select(tree, observer, profile)
    if len(unresolved):
        raise ValueError("requested domain needs finer or original products")
    # Plan the explicitly supplied complete test-motion box before reading fields.
    # This is a finite preparation domain, not a universal continuous reuse radius.
    start = time.perf_counter()
    cut = load_cut(args.product, tree, nodes)
    prepare_s = time.perf_counter() - start
    reference, inputs = prepare_source(source), native_inputs(source)
    frame = context["scene_frame"]
    up = np.array(frame["y_ecef"])
    east = np.array(frame["x_ecef"])
    north = np.array(frame["z_ecef"])
    suns = [up + 0.5 * east + 0.3 * north, up - 0.7 * east - 0.3 * north]
    suns = [s / np.linalg.norm(s) for s in suns]
    reports = {}
    for name, view in poses.items():
        reports[name] = one_view(
            source, reference, inputs, cut, view, suns, args.output / (name + ".npz")
        )
    names = diagnostic_poses(reports, select_full_poses(source, poses))
    picture(names, args.output, args.output / "material-ids.png")
    picture(names, args.output, args.output / "normals.png", normals=True)
    case = next(iter(select_full_poses(source, poses)))
    view = poses[case]
    request = {
        "case": case,
        "profile": profile,
        "provenance": provenance,
        "observer_bounds": observer.tolist(),
        "view": [view[0].tolist(), view[1].tolist(), *view[2:]],
        "roi": reports[case]["roi"],
    }
    (args.output / "cache-request.json").write_text(
        json.dumps(request, indent=2) + "\n"
    )
    geometry = document["results"][0]["geometry"]
    native = (
        24 * len(source["positions"])
        + 12 * len(source["indices"])
        + 4 * (len(geometry["wall_colours"]) + len(geometry["wall_material_metres"]))
    )
    global_plan = global_metadata(args.input, document, observer, profile)
    result = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "binding_check": receipt,
        "profile": profile,
        "observer_bounds": observer.tolist(),
        "cook_wall_s_this_run": assembly,
        "cook": json.loads((args.product / "cook.json").read_text()),
        "product_audit_json_bytes": (args.product / "cook.json").stat().st_size,
        "reference_load_s": reference_load,
        "cut_header_bytes": sum(a.nbytes for a in header.values()),
        "selected_nodes": nodes.tolist(),
        "selection_node_tests": tests,
        "cold_cut_prepare_s": prepare_s,
        "cut_cost": cut["cost"],
        "selected_unsampled_sources": [
            {"source": int(i), "reason": "no sample in the selected field cut"}
            for i in np.setdiff1d(
                header["sources"], np.unique(cut["records"]["source"])
            )
        ],
        "field_reloads_during_120_queries": 0,
        "native_stored_geometry_attribute_bytes": native,
        "exact_native_u16_index_bytes": native - 6 * len(source["indices"]),
        "native_triangles": len(source["indices"]),
        "actual_geometry_extent_m": np.ptp(source["positions"], axis=0).tolist(),
        "global_metadata": global_plan,
        "poses": reports,
        "shown": names,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "source_hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                *Path(__file__).parent.glob("layered_tree_*.py"),
                Path(__file__),
            ]
        },
        "scope": "opaque CPU field-tree proposal; no PBR, continuous coverage/reuse bound, async world publication or native/device budget admission",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in result.items()
                if k
                not in (
                    "cook",
                    "binding_check",
                    "poses",
                    "selected_nodes",
                    "source_hashes",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
