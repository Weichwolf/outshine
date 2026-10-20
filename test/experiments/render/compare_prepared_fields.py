"""Prepared analytic coordinates versus unchanged mesher inputs, with unknowns retained."""

import argparse
import hashlib
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_plane_cloud import image_roi, select_full_poses
from full_view_reference import load, views
from native_capture_meshes import sample
from prepared_field_association import associate, with_geometry_plans
from prepared_material_fields import at_world, facade_catalog
from prepared_surface_recipes import load_recipes
from triangle_image_reference import image_rays, prepare_source


def codes(uv):
    u = np.asarray(uv)[..., 0]
    return np.where(u < 0, u, np.floor(u / 256)).astype(np.int32)


def evaluate(
    source, recipes, face_labels, ids, points, catalog, *, triangle_context=False
):
    uv = np.full((len(ids), 2), np.nan)
    metres = uv.copy()
    known = face_labels[ids] >= 0
    sources = source["binding"][ids, 1]
    for index in np.unique(sources[known]):
        for face in np.unique(face_labels[ids][known & (sources == index)]):
            at = np.flatnonzero(known & (sources == index) & (face_labels[ids] == face))
            surface = recipes[int(index)]
            shape = surface["shapes"][0]
            local = (points[at] - surface["origin"]) @ np.array(
                [surface[k] for k in ("east", "north", "up")]
            ).T
            deck = (
                shape["seat_m"]
                + shape["foot_m"]
                + shape["eaves_m"]
                + (shape["rise_m"] if shape["roof_kind"] == 0 else 0)
            )
            if triangle_context:
                original = (
                    source["positions"][source["indices"][ids[at]]]
                    .astype(float)
                    .mean(1)
                )
                z = (original - surface["origin"]) @ np.array(surface["up"])
            else:
                z = local[:, 2]
            u, m = at_world(
                surface, 0, int(face), points[at], gable=z > deck, catalog=catalog
            )
            uv[at], metres[at] = u, m
    return uv, metres, known


def statistics(
    native_uv, native_m, predicted_uv, predicted_m, known, native_codes=None
):
    actual = codes(native_uv[known]) if native_codes is None else native_codes[known]
    proposed = codes(predicted_uv[known])
    equal = actual == proposed
    delta = predicted_uv[known] - native_uv[known]
    metre = predicted_m[known] - native_m[known]
    return {
        "qualified_points": int(known.sum()),
        "unknown_points": int((~known).sum()),
        "categorical_code_mismatches": int((~equal).sum()),
        "same_code_points": int(equal.sum()),
        "code_delta_counts": {
            str(int(k)): int(n)
            for k, n in zip(
                *np.unique(proposed - actual, return_counts=True), strict=True
            )
        },
        "max_uv_component_error_same_code": np.abs(delta[equal]).max(0).tolist()
        if equal.any()
        else None,
        "max_metre_component_error_all_qualified": np.abs(metre).max(0).tolist()
        if len(metre)
        else None,
        "max_absolute_encoded_u_error": float(np.abs(delta[:, 0]).max())
        if len(delta)
        else None,
    }


def id_colours(code):
    value = np.asarray(code, np.uint32) * np.uint32(2654435761)
    return np.column_stack(
        ((value >> 0) & 255, (value >> 8) & 255, (value >> 16) & 255)
    ).astype(np.uint8)


def picture(rows, path):
    from PIL import Image, ImageDraw

    canvas = Image.new("RGB", (1280, 720), "white")
    draw = ImageDraw.Draw(canvas)
    for row, (name, raw) in enumerate(rows):
        a, b = raw["native_code"], raw["candidate_code"]
        known = raw["qualified"]
        covered = raw["covered"]
        width, height = raw["width"], raw["height"]
        first = id_colours(a)
        second = id_colours(b)
        error = np.full_like(first, 70)
        first[~covered] = 0
        second[~known] = 70
        second[~covered] = 0
        error[known] = [0, 160, 0]
        error[known & (a != b)] = [230, 0, 0]
        error[~covered] = 0
        for col, (label, data) in enumerate(
            (
                ("Native vertex-code input", first),
                ("Analytic candidate; grey unknown", second),
                ("Code difference: red / same: green", error),
            )
        ):
            # Majority display within pixel is only a diagnostic, not PBR rendering.
            image = Image.fromarray(data.reshape(height * 2, width * 2, 3)).resize(
                (width * 8, height * 8), Image.Resampling.NEAREST
            )
            image.thumbnail((410, 165))
            x, y = 426 * col, 240 * row
            canvas.paste(image, (x + 4, y + 35))
            draw.text((x + 4, y + 3), name + " / " + label, fill="black")
            draw.text(
                (x + 4, y + 208),
                f"{width}x{height} px; material-ID diagnostic, no PBR colour",
                fill="black",
            )
    canvas.save(path)


def corner_picture(native, candidate, known, path):
    """Actual boundary counterexamples, including first/provoking input corners."""
    from PIL import Image, ImageDraw

    indices = np.flatnonzero(known)
    bad = indices[codes(native[indices]) != codes(candidate[indices])]
    canvas = Image.new("RGB", (1280, 720), "white")
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (20, 15),
        "Original corner material-code counterexamples; no PBR colour",
        fill="black",
    )
    draw.text(
        (20, 45),
        f"{len(bad)} mismatching corners on {len(np.unique(bad // 3))} original triangles; no clamping",
        fill="black",
    )
    draw.text(
        (20, 80),
        "triangle / corner       original U       analytic U       original code       analytic code",
        fill="black",
    )
    for row, i in enumerate(bad[:15]):
        a, b = int(codes(native[i : i + 1])[0]), int(codes(candidate[i : i + 1])[0])
        y = 115 + row * 35
        draw.text(
            (20, y),
            f"{i // 3:6d} / {i % 3}                  {native[i, 0]:.9f}       {candidate[i, 0]:.9f}              {a}                     {b}",
            fill="black",
        )
        for x, code in ((1010, a), (1080, b)):
            colour = tuple(map(int, id_colours([code])[0]))
            draw.rectangle((x, y, x + 40, y + 24), fill=colour)
    draw.text(
        (20, 675),
        "Re-evaluation at quantized stored positions can cross an encoded ID boundary despite small UV error.",
        fill="black",
    )
    canvas.save(path)


def compare_corners(source, recipes, labels, reasons, catalog, output):
    triangles = source["indices"]
    ids = np.repeat(np.arange(len(triangles)), 3)
    points = source["positions"][triangles].reshape(-1, 3)
    u, m, known = evaluate(
        source, recipes, labels, ids, points, catalog, triangle_context=True
    )
    vertex = statistics(
        source["uv"][triangles].reshape(-1, 2),
        source["metres"][triangles].reshape(-1, 2),
        u,
        m,
        known,
    )
    corner_picture(
        source["uv"][triangles].reshape(-1, 2),
        u,
        known,
        output / "corner-differences.png",
    )
    all_known = known.reshape(-1, 3).all(1)
    candidate_corner_codes = codes(u.reshape(-1, 3, 2)[all_known])
    vertex["associated_triangles_with_nonconstant_candidate_code"] = int(
        (~(candidate_corner_codes == candidate_corner_codes[:, :1]).all(1)).sum()
    )
    vertex["first_corner_code_mismatches"] = int(
        (
            codes(source["uv"][triangles][all_known, 0]) != candidate_corner_codes[:, 0]
        ).sum()
    )
    corner_codes = codes(source["uv"][triangles])
    np.savez_compressed(
        output / "coordinate-corners.npz",
        original_faces=ids,
        qualified=known,
        derived_face=labels[ids],
        native_uv=source["uv"][triangles].reshape(-1, 2),
        native_metres=source["metres"][triangles].reshape(-1, 2),
        candidate_uv=u,
        candidate_metres=m,
        reason=reasons[ids],
    )
    return vertex, corner_codes


def compare_poses(
    source, document, context, recipes, labels, corner_codes, catalog, output
):
    constant = (corner_codes == corner_codes[:, :1]).all(1)
    reference = prepare_source(source)
    reports, rows = {}, []
    poses = views(document, context)
    shown = select_full_poses(source, poses)
    for name, view in poses.items():
        box = image_roi(source, view)
        if box is None:
            reports[name] = {"offscreen": True}
            continue
        width, height = (box[1] - box[0]).astype(int)
        origins, directions = image_rays(view, box, width, height, 2)
        depth, owner, fields = sample(source, reference, origins, directions)
        covered = owner >= 0
        ids = owner[covered]
        points = origins[covered] + depth[covered, None] * directions[covered]
        uv, metres, qualified = evaluate(source, recipes, labels, ids, points, catalog)
        qualified &= constant[ids]
        codes_native = corner_codes[ids, 0]
        reports[name] = statistics(
            fields["uv"][covered],
            fields["metres"][covered],
            uv,
            metres,
            qualified,
            native_codes=codes_native,
        )
        a = np.zeros(len(owner), np.int32)
        b = a.copy()
        mask = np.zeros(len(owner), bool)
        a[covered] = codes_native
        b[np.flatnonzero(covered)[qualified]] = codes(uv[qualified])
        mask[covered] = qualified
        raw = {
            "native_code": a,
            "candidate_code": b,
            "qualified": mask,
            "covered": covered,
            "width": int(width),
            "height": int(height),
            "native_uv": fields["uv"],
            "native_metres": fields["metres"],
            "original_owner": owner,
            "candidate_uv_on_covered": uv,
            "candidate_metres_on_covered": metres,
        }
        if name in shown:
            rows.append((name, raw))
        np.savez_compressed(output / (name + ".npz"), **raw)
    picture(rows, output / "comparison.png")
    return reports


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    tracemalloc.start()
    start = time.perf_counter()
    recipes, cost = load_recipes(args.input, document)
    labels, reasons, counts = associate(source, with_geometry_plans(recipes, document))
    catalog = facade_catalog()
    decode_cost = {
        "wall_s": time.perf_counter() - start,
        "traced_peak_bytes": tracemalloc.get_traced_memory()[1],
    }
    tracemalloc.stop()
    vertex, corner_codes = compare_corners(
        source, recipes, labels, reasons, catalog, args.output
    )
    reports = compare_poses(
        source, document, context, recipes, labels, corner_codes, catalog, args.output
    )
    triangles = source["indices"]
    constant = (corner_codes == corner_codes[:, :1]).all(1)
    geometry = document["results"][0]["geometry"]
    stored = (
        24 * len(source["positions"])
        + 12 * len(triangles)
        + 4 * (len(geometry["wall_colours"]) + len(geometry["wall_material_metres"]))
    )
    result = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "source_hashes": {
            n: hashlib.sha256((Path(__file__).parent / n).read_bytes()).hexdigest()
            for n in (
                "prepared_surface_recipes.py",
                "prepared_material_fields.py",
                "prepared_field_association.py",
                "compare_prepared_fields.py",
            )
        },
        "scope": "existing analytic BuildingSurface rules, geometric association control only; original native Part/Face remain unknown; no PBR/GPU admission",
        "source_count": len(recipes),
        "shape_count": sum(len(s["shapes"]) for s in recipes.values() if s),
        "binding_check": receipt,
        "recipe_block": cost,
        "native_stored_geometry_attribute_bytes": stored,
        "unqualified_gpu_layout": True,
        "association_reasons": dict(counts),
        "constant_native_code_triangles": int(constant.sum()),
        "triangles": len(triangles),
        "vertex_coordinates": vertex,
        "poses": reports,
        "sequence_totals": {
            "poses": len(reports),
            "offscreen": sum(r.get("offscreen", False) for r in reports.values()),
            **{
                k: sum(r.get(k, 0) for r in reports.values())
                for k in (
                    "qualified_points",
                    "unknown_points",
                    "categorical_code_mismatches",
                )
            },
        },
        "native_rule_sha256": {
            n: hashlib.sha256(
                (Path(__file__).resolve().parents[3] / n).read_bytes()
            ).hexdigest()
            for n in (
                "src/generators/building/BuildingSurface.cpp",
                "src/generators/building/BuildingSurfacePatches.cpp",
                "src/generators/building/FacadeUv.h",
                "src/generators/building/BuildingWallMaterial.h",
                "src/content/shade/FacadeProfiles.inc",
                "src/render/shaders/litVertex.glsl",
            )
        },
        "unmeasured_costs": [
            "allocation count",
            "physical bytes copied/traffic",
            "GPU time",
            "native decoded/resident payload",
        ],
        "decode_and_association_cost": decode_cost,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in result.items()
                if k not in ("source_hashes", "binding_check")
            }
        )
    )


if __name__ == "__main__":
    main()
