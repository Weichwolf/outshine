"""Original full-cell controls for continuous camera-domain surface support."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from attributed_prisms import native_inputs
from check_viewcell_support import check_witnesses
from compare_attributed_prisms import diagnostic_poses, picture
from compare_layered_tree import one_view
from compare_plane_cloud import select_full_poses
from full_view_reference import load, views
from triangle_image_reference import prepare_source
from viewcell_product import compact, cook, load_product, query
from viewcell_visibility import select_support


def original_weld_control(source):
    # Equality includes bit patterns of all original fields, not just positions.
    words = np.column_stack(
        [
            source[k].reshape(len(source["positions"]), -1).view(np.uint32)
            for k in ("positions", "uv", "normal", "metres", "colour")
        ]
    )
    used = np.unique(source["indices"])
    return {
        "used_original_vertices": len(used),
        "bit_identical_full_attribute_vertices": len(np.unique(words[used], axis=0)),
        "words_per_vertex": words.shape[1],
    }


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ("input", "product", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    temp = Path(tempfile.gettempdir()).resolve()
    repo = Path(__file__).resolve().parents[3]
    if any(
        not p.resolve().is_relative_to(temp) or p.resolve().is_relative_to(repo)
        for p in (args.product, args.output)
    ):
        parser.error("private products/reports belong in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    poses = views(document, context)
    eyes = np.array([view[0] for view in poses.values()])
    observer = np.stack((eyes.min(0), eyes.max(0)))
    first = next(iter(poses.values()))
    profile = {"width": int(first[3]), "height": int(first[4]), "focal_px": first[2]}
    provenance = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "parent": document["parent"],
        "view": document["view_key"],
    }
    start = time.perf_counter()
    baseline = select_support(
        source["positions"], source["indices"], observer, max_blockers=0
    )
    baseline_s = time.perf_counter() - start
    baseline_fields = compact(source, np.flatnonzero(baseline.keep))
    baseline_bytes = {k: a.nbytes for k, a in baseline_fields.items()}
    del baseline_fields
    start = time.perf_counter()
    support = cook(source, observer, args.product, provenance, profile)
    cook_s = time.perf_counter() - start
    proof = check_witnesses(source, support, observer)
    np.savez_compressed(
        args.output / "witnesses.npz",
        witness=support.witness,
        keep=support.keep,
        blockers=support.blockers,
        observer_box=observer,
    )
    start = time.perf_counter()
    product = load_product(args.product, observer, provenance)
    prepare_s = time.perf_counter() - start
    reference, inputs = prepare_source(source), native_inputs(source)
    frame = context["scene_frame"]
    up, east, north = (np.array(frame[k]) for k in ("y_ecef", "x_ecef", "z_ecef"))
    suns = [up + 0.5 * east + 0.3 * north, up - 0.7 * east - 0.3 * north]
    reports = {}
    random = np.random.default_rng(8992000)
    regular = select_full_poses(source, poses)
    anchor_view = poses[next(iter(regular))]
    for i, eye in enumerate(random.uniform(observer[0], observer[1], (8, 3))):
        poses[f"interior-{i:03d}"] = (eye, *anchor_view[1:])
    for name, view in poses.items():
        reports[name] = one_view(
            source,
            reference,
            inputs,
            product,
            view,
            suns,
            args.output / (name + ".npz"),
            query_fn=query,
        )
    shown = diagnostic_poses(reports, regular)
    picture(shown, args.output, args.output / "material-ids.png")
    picture(shown, args.output, args.output / "normals.png", normals=True)
    case = next(iter(regular))
    view = poses[case]
    request = {
        "case": case,
        "provenance": provenance,
        "view": [view[0].tolist(), view[1].tolist(), *view[2:]],
        "roi": reports[case]["roi"],
    }
    (args.output / "cache-request.json").write_text(
        json.dumps(request, indent=2) + "\n"
    )
    native = 24 * len(source["positions"]) + 12 * len(source["indices"])
    geometry = document["results"][0]["geometry"]
    native += 4 * (
        len(geometry["wall_colours"]) + len(geometry["wall_material_metres"])
    )
    result = {
        "profile": profile,
        "provenance": provenance,
        "binding_check": receipt,
        "observer_bounds": observer.tolist(),
        "cook_wall_s": cook_s,
        "cold_product_prepare_s": prepare_s,
        "product": product["header"],
        "product_cpu_array_bytes": {
            k: product[k]
            for k in (
                "stored_array_bytes",
                "decoded_normal_bytes",
                "vertex_stage_copy_bytes",
            )
        },
        "backface_only": {
            "selection_wall_s": baseline_s,
            "arrays": baseline_bytes,
            "work": baseline.work,
        },
        "native_core_bytes": native,
        "native_u16_core_bytes": native - 6 * len(source["indices"]),
        "original_weld_control": original_weld_control(source),
        "independent_witness_checks": proof,
        "poses": reports,
        "shown": shown,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "source_hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                Path(__file__),
                *(
                    Path(__file__).with_name(name)
                    for name in (
                        "viewcell_visibility.py",
                        "viewcell_product.py",
                        "check_viewcell_support.py",
                        "bench_viewcell_product.py",
                        "compare_layered_tree.py",
                    )
                ),
                repo / "lib/src/outshine_ref/geometry/occlusion.py",
            ]
        },
        "scope": "opaque camera-only support; original caster/logical assets remain; no projected field, global amortization, frame or device admission",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in result.items()
                if k not in ("poses", "source_hashes", "binding_check")
            }
        )
    )


if __name__ == "__main__":
    main()
