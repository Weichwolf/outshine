"""Conditional view cubature against unchanged full filtered GGX; #918."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from area_measure_bake import cook
from area_measure_moments import program as moment_program
from area_measure_render import evaluate, program
from bench_area_measure import load_product
from compare_area_measure import picture, reference_metadata
from compare_plane_cloud import image_roi
from compare_visible_normals import inputs
from visible_normal_compare import geometry_controls, metrics
from visible_normal_material import tables


def prepare(implementation, arrays, view, box):
    start = time.perf_counter()
    result = implementation(arrays, arrays, view, box)
    return result, {
        "prepare_s": time.perf_counter() - start,
        "program_array_bytes": sum(
            a.nbytes for a in result.values() if isinstance(a, np.ndarray)
        ),
        "work": result["work"],
    }


def compare_pose(arrays, view, box, lights, energy, name, output, baseline_path):
    full, full_cost = prepare(program, arrays, view, box)
    compact, compact_cost = prepare(moment_program, arrays, view, box)
    result = {
        "roi": box.tolist(),
        "full": full_cost,
        "moments": compact_cost,
        "cases": {},
    }
    for sun, light in lights.items():
        for roughness in (0.12, 0.45):
            key = f"{sun}-r{str(roughness).replace('.', 'p')}"
            start = time.perf_counter()
            reference, coverage_a = evaluate(
                full, box[1] - box[0], light, roughness, energy
            )
            full_s = time.perf_counter() - start
            start = time.perf_counter()
            candidate, coverage_b = evaluate(
                compact, box[1] - box[0], light, roughness, energy
            )
            compact_s = time.perf_counter() - start
            error = {
                "kernel_delta": metrics(reference, candidate),
                "coverage_max_abs": float(abs(coverage_a - coverage_b).max()),
                "coverage_l1_px": float(abs(coverage_a - coverage_b).sum()),
                "full_shade_s": full_s,
                "moment_shade_s": compact_s,
            }
            if baseline_path:
                with np.load(
                    baseline_path / f"{name}-{key}.npz", allow_pickle=False
                ) as saved:
                    np.testing.assert_array_equal(reference, saved["candidate"])
                    np.testing.assert_array_equal(
                        coverage_a, saved["candidate_coverage"]
                    )
                    error["full_against_original"] = metrics(saved["fine"], reference)
                    error["moments_against_original"] = metrics(
                        saved["fine"], candidate
                    )
                error["baseline_arrays_bit_equal"] = True
            result["cases"][key] = error
            np.savez_compressed(
                output / f"{name}-{key}.npz",
                full=reference,
                candidate=candidate,
                full_coverage=coverage_a,
                candidate_coverage=coverage_b,
            )
            if sun == "day" and roughness == 0.12:
                picture(
                    output / (name + ".png"),
                    reference,
                    candidate,
                    reference_label="full filtered field GGX",
                    candidate_label="conditional view moments GGX",
                )
    return result


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ("input", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--product", type=Path)
    parser.add_argument("--baseline-report", type=Path)
    parser.add_argument("--all-poses", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    (
        source,
        document,
        context,
        receipt,
        poses,
        selected,
        observers,
        domain,
        profile,
        provenance,
    ) = inputs(args.input)
    read_s = time.perf_counter() - start
    up, east, north = (
        np.array(context["scene_frame"][k]) for k in ("y_ecef", "x_ecef", "z_ecef")
    )
    start = time.perf_counter()
    if args.product:
        arrays, header = load_product(args.product, provenance, observers)
    else:
        controls = geometry_controls(source, document["sources"], domain, output)
        arrays, header = cook(
            source,
            domain,
            up,
            profile,
            document["sources"],
            output / "product",
            provenance,
            controls["backface_only"]["appearance_array_bytes"],
        )
    product_s = time.perf_counter() - start
    metadata = reference_metadata(
        args.baseline_report, source, poses, domain, profile, provenance
    )
    if args.baseline_report:
        with np.load(
            args.baseline_report.parent / "product/measure.npz", allow_pickle=False
        ) as old:
            for key, value in arrays.items():
                np.testing.assert_array_equal(value, old[key])
    report = {
        "product": header,
        "binding_check": receipt,
        "input_read_s": read_s,
        "product_load_or_build_s": product_s,
        "poses": {},
        "scope": "query-only comparison on the unchanged #917 workload. No field-quality, native, geometry/depth/material or whole-world admission",
    }
    lights = {
        "day": up + 0.5 * east + 0.3 * north,
        "side": up - 0.7 * east - 0.3 * north,
    }
    energy = tables()
    for name, view in (poses if args.all_poses else selected).items():
        box = image_roi(source, view)
        if box is None:
            report["poses"][name] = {"offscreen": True}
            continue
        if (
            metadata["poses"][name].get("offscreen")
            or metadata["poses"][name]["roi"] != box.tolist()
        ):
            raise ValueError("baseline has different pixel support")
        result = compare_pose(
            arrays,
            view,
            box,
            lights,
            energy,
            name,
            output,
            args.baseline_report.parent if args.baseline_report else None,
        )
        report["poses"][name] = result
        print(
            json.dumps(
                {
                    "pose": name,
                    "full_records": result["full"]["work"]["quadrature_records"],
                    "moment_records": result["moments"]["work"]["quadrature_records"],
                    "max_kernel_error": max(
                        c["kernel_delta"]["linear_max_abs"]
                        for c in result["cases"].values()
                    ),
                }
            ),
            flush=True,
        )
    first = next(iter(selected))
    request = {
        "view": [np.asarray(x).tolist() for x in poses[first]],
        "box": image_roi(source, poses[first]).tolist(),
        "provenance": provenance,
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
