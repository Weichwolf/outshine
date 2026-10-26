"""Original unchanged-profile poses against an interval-selected directional field."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from attributed_prisms import native_inputs
from beam_field_product import cook, load_product, query
from compare_attributed_prisms import diagnostic_poses, picture
from compare_layered_tree import one_view
from compare_plane_cloud import select_full_poses
from compare_projective_field import factorized_control
from full_view_reference import load, views
from triangle_image_reference import prepare_source


def control(source):
    lineage, mapping = np.unique(source["binding"][:, :5], axis=0, return_inverse=True)
    arrays = {
        "lineage": lineage,
        "lineage_index": mapping.astype(
            np.uint16 if len(lineage) <= 65536 else np.uint32
        ),
        "original_face": np.arange(len(source["indices"]), dtype=np.uint32),
        "native_face_tag": source["binding"][:, 5],
    }
    _, report = factorized_control(source, arrays)
    return report


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    if not args.output.resolve().is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or args.output.resolve().is_relative_to(repo):
        parser.error("private output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    poses = views(document, context)
    observers = np.array([view[0] for view in poses.values()])
    domain = np.stack((observers.min(0), observers.max(0)))
    first = next(iter(poses.values()))
    profile = {"width": int(first[3]), "height": int(first[4]), "focal_px": first[2]}
    provenance = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "parent": document["parent"],
        "view": document["view_key"],
    }
    selected = select_full_poses(source, poses)
    orient = next(iter(selected.values()))
    for i in range(8):
        reverse = int(f"{i:03b}"[::-1], 2)
        fraction = np.array([(i + 0.5) / 8, (reverse + 0.5) / 8, 0.5])
        eye = domain[0] + fraction * np.ptp(domain, axis=0)
        poses[f"domain-interior-{i}"] = eye, *orient[1:]
    up, east, north = (
        np.array(context["scene_frame"][key]) for key in ("y_ecef", "x_ecef", "z_ecef")
    )
    suns = [up + 0.5 * east + 0.3 * north, up - 0.7 * east - 0.3 * north]
    start = time.perf_counter()
    header = cook(
        source,
        domain,
        profile,
        args.output / "product",
        provenance,
        certified_opaque=receipt["opaque_assumption"],
    )
    bake_s = time.perf_counter() - start
    start = time.perf_counter()
    product = load_product(args.output / "product", observers, provenance)
    load_s = time.perf_counter() - start
    prepared, inputs = prepare_source(source), native_inputs(source)
    reports = {
        name: one_view(
            source,
            prepared,
            inputs,
            product,
            view,
            suns,
            args.output / (name + ".npz"),
            query_fn=query,
        )
        for name, view in poses.items()
    }
    shown = diagnostic_poses(reports, selected)
    picture(shown, args.output, args.output / "material-ids.png")
    picture(shown, args.output, args.output / "normals.png", normals=True)
    name = next(iter(selected))
    view = poses[name]
    request = {
        "case": name,
        "view": [view[0].tolist(), view[1].tolist(), *view[2:]],
        "roi": reports[name]["roi"],
        "provenance": provenance,
    }
    (args.output / "request.json").write_text(json.dumps(request, indent=2) + "\n")
    report = {
        "product": header,
        "bake_s": bake_s,
        "load_s": load_s,
        "derived_cpu_bytes": product["derived_cpu_bytes"],
        "vertex_stage_copy_bytes": product["vertex_stage_copy_bytes"],
        "query_work": product["work"],
        "exact_control": control(source),
        "native_reference": receipt,
        "poses": reports,
        "shown": shown,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocation_count": None,
        "all_copied_bytes": None,
        "scope": "one supplied native group and actual observer domain; no original caster deletion, extra world products, construction reserve, native PBR/frame or global amortization claim",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    visible = [r for r in reports.values() if not r.get("offscreen")]
    print(
        json.dumps(
            {
                "field_bytes": header["stored_array_bytes"],
                "exact_control_bytes": report["exact_control"]["total_array_bytes"],
                "poses": len(poses),
                "visible_poses": len(visible),
                "lost": sum(r["lost_samples"] for r in visible),
                "added": sum(r["added_samples"] for r in visible),
                "work": product["work"],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
