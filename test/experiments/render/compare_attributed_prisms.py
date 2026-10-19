"""Complete stored procedural geometry+material inputs against unchanged native fields."""

import argparse
import hashlib
import itertools
import json
import resource
from pathlib import Path

import numpy as np
from attributed_prisms import (
    compile_product,
    load_product,
    native_fields,
    native_inputs,
    render_product,
)
from compare_plane_cloud import image_roi, select_full_poses
from compare_roof_prisms import measured
from full_view_reference import load, views
from native_capture_meshes import independent_depth
from prepared_surface_recipes import load_recipes
from triangle_image_reference import (
    depth_difference,
    image_rays,
    intersect_source,
    prepare_source,
)


def field_errors(original, candidate, common, same_source):
    equal = original["code"] == candidate["code"]
    same = common & same_source & equal
    normals = np.clip(np.sum(original["normal"] * candidate["normal"], axis=1), -1, 1)
    angle = np.degrees(np.arccos(normals[common]))
    return {
        "common_hits": int(common.sum()),
        "different_source_hits": int((common & ~same_source).sum()),
        "different_code_hits_all_common": int((common & ~equal).sum()),
        "different_code_hits_same_source": int((common & same_source & ~equal).sum()),
        "same_source_code_hits": int(same.sum()),
        "max_normal_angle_all_common_deg": float(angle.max()) if len(angle) else None,
        "p95_normal_angle_all_common_deg": float(np.percentile(angle, 95))
        if len(angle)
        else None,
        **{
            f"max_{k}_component_error_same_source_code": np.abs(
                candidate[k][same] - original[k][same]
            )
            .max(0)
            .tolist()
            if same.any()
            else None
            for k in ("coordinates", "metres", "colour")
        },
    }


def check_view(source, reference, inputs, product, view, extent, output):
    box = image_roi(extent, view)
    if box is None:
        return {"offscreen": True}
    width, height = (box[1] - box[0]).astype(int)
    if width * height * 4 > 4194304:
        raise ValueError("finite full-resolution ROI budget exceeded")
    origins, directions = image_rays(view, box, width, height, 2)
    a_depth, a_face = intersect_source(source, origins, directions, prepared=reference)
    original = native_fields(inputs, origins, directions, a_depth, a_face)
    result, cost = measured(lambda: render_product(product, origins, directions))
    b_depth, candidate, b_source, b_part, owner, plane, fallback, work = result
    a_hit, b_hit = np.isfinite(a_depth), np.isfinite(b_depth)
    a_source = np.full(len(a_face), -1, np.int32)
    a_source[a_hit] = source["binding"][a_face[a_hit], 1]
    common = a_hit & b_hit
    # Independent all-triangle geometry controls reuse the original native input.
    selected = np.unique(
        np.linspace(0, len(origins) - 1, min(16, len(origins))).astype(int)
    )
    brute = independent_depth(
        source["positions"][source["indices"]], origins[selected], directions[selected]
    )
    np.testing.assert_array_equal(np.isfinite(brute), a_hit[selected])
    finite = np.isfinite(brute)
    np.testing.assert_allclose(
        brute[finite], a_depth[selected][finite], atol=1e-8, rtol=1e-10
    )
    report = {
        "roi": box.tolist(),
        "width": int(width),
        "height": int(height),
        "subsamples": len(a_face),
        "original_hits": int(a_hit.sum()),
        "candidate_hits": int(b_hit.sum()),
        "lost_samples": int((a_hit & ~b_hit).sum()),
        "added_samples": int((~a_hit & b_hit).sum()),
        "alpha_l1_px": float(abs(a_hit.astype(int) - b_hit.astype(int)).sum() / 4),
        "native_unknown_part_common": int(
            (source["binding"][a_face[common], 2] < 0).sum()
        ),
        "candidate_analytic_common": int((common & (owner >= 0)).sum()),
        "candidate_fallback_hits": int((fallback >= 0).sum()),
        "fields": field_errors(original, candidate, common, a_source == b_source),
        "depth": depth_difference(a_depth, b_depth, np.ones(len(a_face), bool)),
        "work": work,
        "candidate_cost": cost,
        "independent_native_rays": len(selected),
    }
    raw = {
        "original_depth": a_depth,
        "candidate_depth": b_depth,
        "original_face": a_face,
        "original_source": a_source,
        "candidate_source": b_source,
        "candidate_part": b_part,
        "candidate_primitive": owner,
        "candidate_plane": plane,
        "fallback_original_face": fallback,
        "width": width,
        "height": height,
    }
    raw.update({"original_" + k: v for k, v in original.items()})
    raw.update({"candidate_" + k: v for k, v in candidate.items()})
    np.savez_compressed(output, **raw)
    return report


def picture(names, directory, output, normals=False):
    from compare_prepared_fields import id_colours
    from PIL import Image, ImageDraw

    canvas = Image.new("RGB", (1280, 240 * len(names)), "white")
    draw = ImageDraw.Draw(canvas)
    for row, name in enumerate(names):
        with np.load(directory / (name + ".npz")) as raw:
            width, height = int(raw["width"]), int(raw["height"])
            a, b = (
                np.isfinite(raw["original_depth"]),
                np.isfinite(raw["candidate_depth"]),
            )
            if normals:
                first = np.rint(
                    np.clip((raw["original_normal"] + 1) / 2, 0, 1) * 255
                ).astype(np.uint8)
                second = np.rint(
                    np.clip((raw["candidate_normal"] + 1) / 2, 0, 1) * 255
                ).astype(np.uint8)
                delta = np.linalg.norm(
                    raw["candidate_normal"] - raw["original_normal"], axis=1
                )
                error = np.column_stack(
                    (np.minimum(delta * 1000, 255), np.zeros((len(a), 2)))
                ).astype(np.uint8)
            else:
                first, second = (
                    id_colours(raw["original_code"]),
                    id_colours(raw["candidate_code"]),
                )
                error = np.zeros_like(first)
                error[a & b] = [0, 150, 0]
                error[a & b & (raw["original_code"] != raw["candidate_code"])] = [
                    230,
                    0,
                    0,
                ]
            error[a & ~b] = [255, 0, 255]
            error[b & ~a] = [0, 180, 255]
            first[~a], second[~b] = 0, 0
            for col, (label, data) in enumerate(
                zip(
                    ("native inputs", "stored procedural inputs", "difference"),
                    (first, second, error),
                    strict=True,
                )
            ):
                image = Image.fromarray(data.reshape(height * 2, width * 2, 3))
                image = image.resize((width * 8, height * 8), Image.Resampling.NEAREST)
                image.thumbnail((410, 165))
                x, y = 426 * col, 240 * row
                canvas.paste(image, (x + 4, y + 35))
                draw.text((x + 4, y + 3), name + " / " + label, fill="black")
                draw.text(
                    (x + 4, y + 210),
                    f"{width}x{height}px; "
                    + ("normal RGB" if normals else "hashed material ID")
                    + "; no PBR",
                    fill="black",
                )
    canvas.save(output)


def payload_report(arrays, source, document, product):
    geometry = document["results"][0]["geometry"]
    native = (
        24 * len(source["positions"])
        + 12 * len(source["indices"])
        + 4 * (len(geometry["wall_colours"]) + len(geometry["wall_material_metres"]))
    )
    narrowed = (
        native - 6 * len(source["indices"])
        if len(source["positions"]) <= 65536
        else native
    )
    fallback = product[3]
    derived = fallback["vertex_normal"].nbytes
    if product[4] is not None:
        derived += product[4][1]["uv"].nbytes + product[4][1]["flat_codes"].nbytes
    if not len(arrays["fallback_colour"]):
        derived += 16
    return {
        "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
        "array_bytes": {k: a.nbytes for k, a in arrays.items()},
        "derived_fallback_attribute_bytes": derived,
        "stored_plus_derived_attribute_bytes": sum(a.nbytes for a in arrays.values())
        + derived,
        "native_stored_geometry_attribute_bytes": native,
        "same_original_u16_index_bytes": narrowed,
        "unqualified_gpu_layout": True,
        "excluded_costs": [
            "NPZ container overhead (reported separately)",
            "Rtree/native library acceleration allocations",
            "query/output arrays",
            "native GPU resources",
            "physical traffic/copy bytes/allocation counts",
        ],
    }


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    recipes, wire = load_recipes(args.input, document)
    (arrays, reasons), assembly = measured(
        lambda: compile_product(document, source, recipes)
    )
    np.savez_compressed(args.output / "representation.npz", **arrays)
    product, reload_cost = measured(
        lambda: load_product(args.output / "representation.npz")
    )
    prisms = product[0]
    represented = set(map(int, prisms.binding[:, 0]))
    if represented & set(reasons) or represented | set(reasons) != set(recipes):
        raise ValueError("complete original source partition required")
    corners = np.array(
        [
            list(itertools.product(*zip(box[0], box[1], strict=True)))
            for box in prisms.bounds
        ]
    ).reshape(-1, 3)
    extent = {"positions": np.concatenate((source["positions"], corners))}
    reference, inputs = prepare_source(source), native_inputs(source)
    poses = views(document, context)
    reports = {}
    for name, view in poses.items():
        reports[name] = check_view(
            source,
            reference,
            inputs,
            product,
            view,
            extent,
            args.output / (name + ".npz"),
        )
    shown = list(select_full_poses(source, poses))
    visible = {k: v for k, v in reports.items() if not v.get("offscreen")}
    worst = max(
        visible,
        key=lambda k: (
            visible[k]["fields"]["different_code_hits_all_common"]
            + visible[k]["lost_samples"]
            + visible[k]["added_samples"]
        ),
    )
    if worst not in shown:
        shown.append(worst)
    picture(shown, args.output, args.output / "material-ids.png")
    picture(shown, args.output, args.output / "normals.png", normals=True)
    result = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "binding_check": receipt,
        "recipe_wire": wire,
        "assembly": assembly,
        "reload": reload_cost,
        "source_count": len(recipes),
        "represented_sources": len(represented),
        "primitive_count": len(prisms.binding),
        "fallback_reasons": reasons,
        "payload": payload_report(arrays, source, document, product),
        "npz_container_bytes": (args.output / "representation.npz").stat().st_size,
        "poses": reports,
        "shown": shown,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "source_hashes": {
            n: hashlib.sha256((Path(__file__).parent / n).read_bytes()).hexdigest()
            for n in (
                "attributed_prisms.py",
                "compare_attributed_prisms.py",
                "prepared_surface_recipes.py",
                "primitive_bvh.py",
                "roof_prisms.py",
                "native_capture_meshes.py",
            )
        },
        "scope": "opaque Double point-sampled geometry/material inputs only; no actual native draw bindings, PBR shading, raster derivatives/TAA, device budget or world admission",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in result.items()
                if k not in ("binding_check", "poses", "source_hashes")
            }
        )
    )


if __name__ == "__main__":
    main()
