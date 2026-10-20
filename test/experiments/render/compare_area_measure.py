"""Adaptive anchored measure versus saved analytic #912 area references."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from area_measure_bake import cook
from area_measure_render import evaluate, program
from compare_visible_normals import inputs
from opaque_area_reference import evaluate as reference_evaluate
from opaque_area_reference import pixel_grid, quadrature
from PIL import Image, ImageDraw
from visible_normal_compare import display, geometry_controls, metrics
from visible_normal_material import tables


def picture(path, fine, candidate):
    canvas = Image.new("RGB", (1200, 250), "white")
    draw = ImageDraw.Draw(canvas)
    for i, (label, data) in enumerate(
        (
            ("analytic original GGX", display(fine)),
            ("adaptive anchored measure GGX", display(candidate)),
            (
                "display error x8",
                np.clip(8 * abs(display(fine) - display(candidate)), 0, 1),
            ),
        )
    ):
        image = Image.fromarray(np.uint8(np.clip(data, 0, 1) * 255 + 0.5))
        image = image.resize(
            (380, max(1, int(380 * image.height / image.width))),
            Image.Resampling.NEAREST,
        )
        canvas.paste(image, (400 * i + 5, 40))
        draw.text((400 * i + 5, 5), label, fill="black")
    canvas.save(path)


def compare_pose(
    arrays,
    source,
    view,
    box,
    reference,
    name,
    lights,
    energy,
    output,
    expected_cases,
    *,
    refinement_check,
):
    start = time.perf_counter()
    p = program(arrays, arrays, view, box)
    shape = box[1] - box[0]
    result = {
        "roi": box.tolist(),
        "program_s": time.perf_counter() - start,
        "work": p["work"],
        "program_array_bytes": sum(
            v.nbytes for v in p.values() if isinstance(v, np.ndarray)
        ),
        "cases": {},
    }
    reference_program = None
    start = time.perf_counter()
    if not all(
        (reference / f"{name}-{sun}-r{str(r).replace('.', 'p')}.npz").exists()
        for sun in lights
        for r in (0.12, 0.45)
    ):
        reference_program = quadrature(
            list(pixel_grid(source, view, box).fragments()), view, box, refined=True
        )
    result["original_reference_prepare_s"] = time.perf_counter() - start
    fine_program = (
        program(arrays, arrays, view, box, refined=True) if refinement_check else None
    )
    for sun, light in lights.items():
        for roughness in (0.12, 0.45):
            case = f"{sun}-r{str(roughness).replace('.', 'p')}"
            path = reference / f"{name}-{case}.npz"
            if path.exists():
                with np.load(path, allow_pickle=False) as original:
                    fine = original["fine"]
                    original_coverage = original["original_coverage"]
                reference_hash = hashlib.sha256(path.read_bytes()).hexdigest()
            else:
                fine, original_coverage = reference_evaluate(
                    reference_program, source, shape, light, roughness, energy
                )
                reference_hash = None
            np.testing.assert_allclose(
                float(original_coverage.sum()),
                expected_cases[case]["reference_coverage_px"],
                atol=1e-8,
                rtol=1e-11,
            )
            start = time.perf_counter()
            rgb, coverage = evaluate(p, shape, light, roughness, energy)
            shade_s = time.perf_counter() - start
            values = metrics(fine, rgb)
            values.update(
                coverage_l1_px=float(abs(original_coverage - coverage).sum()),
                coverage_max=float(abs(original_coverage - coverage).max()),
                reference_coverage_px=float(original_coverage.sum()),
                shade_s=shade_s,
                reference_array_file_sha256=reference_hash,
                reference_origin="saved independent original arrays"
                if reference_hash
                else "recomputed analytic original, 12-point fragment quadrature",
            )
            if fine_program is not None:
                refined, refined_coverage = evaluate(
                    fine_program, shape, light, roughness, energy
                )
                values["quadrature_refinement"] = metrics(rgb, refined)
                values["quadrature_coverage_max"] = float(
                    abs(coverage - refined_coverage).max()
                )
            result["cases"][case] = values
            np.savez_compressed(
                output / f"{name}-{case}.npz",
                fine=fine,
                candidate=rgb,
                original_coverage=original_coverage,
                candidate_coverage=coverage,
            )
            if sun == "day" and roughness == 0.12:
                picture(output / (name + ".png"), fine, rgb)
    return result


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ("input", "area_reference", "output"):
        parser.add_argument("--" + name.replace("_", "-"), type=Path, required=True)
    parser.add_argument("--all-poses", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
    (
        source,
        document,
        context,
        receipt,
        poses,
        selected,
        _observers,
        domain,
        profile,
        provenance,
    ) = inputs(args.input)
    reference = json.loads(args.area_reference.read_text())
    if (
        reference["product"]["provenance"] != provenance
        or reference["product"]["observer_bounds"] != domain.tolist()
        or reference["product"]["profile"] != profile
        or set(reference["poses"]) != set(poses)
    ):
        raise ValueError(
            "area reference belongs to another input/domain/profile/pose set"
        )
    controls = geometry_controls(source, document["sources"], domain, output)
    limit = controls["backface_only"]["appearance_array_bytes"]
    up, east, north = (
        np.array(context["scene_frame"][k]) for k in ("y_ecef", "x_ecef", "z_ecef")
    )
    arrays, header = cook(
        source,
        domain,
        up,
        profile,
        document["sources"],
        output / "product",
        provenance,
        limit,
    )
    print(
        json.dumps(
            {
                "cooked": header["stored_array_bytes"],
                "leaves": header["adaptive_work"]["leaves"],
            }
        ),
        flush=True,
    )
    report = {
        "product": header,
        "geometry_controls": controls,
        "binding_check": receipt,
        "reused_area_report_sha256": hashlib.sha256(
            args.area_reference.read_bytes()
        ).hexdigest(),
        "poses": {},
    }
    energy = tables()
    lights = {
        "day": up + 0.5 * east + 0.3 * north,
        "side": up - 0.7 * east - 0.3 * north,
    }
    for name, view in (poses if args.all_poses else selected).items():
        if reference["poses"][name].get("offscreen"):
            report["poses"][name] = {"offscreen": True}
            continue
        result = compare_pose(
            arrays,
            source,
            view,
            np.array(reference["poses"][name]["roi"]),
            args.area_reference.parent,
            name,
            lights,
            energy,
            output,
            reference["poses"][name]["cases"],
            refinement_check=name == next(iter(selected)),
        )
        report["poses"][name] = result
        print(
            json.dumps(
                {
                    "pose": name,
                    "max_linear_error": max(
                        c["linear_max_abs"] for c in result["cases"].values()
                    ),
                }
            ),
            flush=True,
        )
    first = next(iter(selected))
    view = [np.asarray(v).tolist() for v in poses[first]]
    request = {
        "provenance": provenance,
        "view": view,
        "box": reference["poses"][first]["roi"],
        "light": lights["day"].tolist(),
        "roughness": 0.12,
    }
    (output / "consumer-request.json").write_text(json.dumps(request) + "\n")
    report["process_peak_rss_bytes"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({"terminal": "complete", "poses": len(report["poses"])}), flush=True
    )


if __name__ == "__main__":
    main()
