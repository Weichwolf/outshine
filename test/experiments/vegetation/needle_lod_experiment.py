"""Compare closed needles with position envelopes and independent coverage samples."""

import argparse
import hashlib
import json
import subprocess
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from demand import Camera
from needle_checks import check_mesh
from needle_lod_bounds import independent_interpolation_samples, projected_mesh_bound
from needle_projected_area import SNAP_GRID_M, projected_areas
from outshine_ref.vegetation import Needle, needle_surface

BASE = Path("build/experiments/vegetation/europe-conifers/tile/tile.json").resolve()
OUTPUT = Path("build/experiments/vegetation/needle-lod")
UNIFORM_CANDIDATES = tuple(
    (segments, sides, "uniform")
    for segments, sides in ((2, 4), (4, 4), (8, 4), (8, 8), (16, 8), (24, 16))
)
CANDIDATES = (
    *UNIFORM_CANDIDATES,
    (4, 8, "uniform"),
    *((segments, sides, "profile") for segments in (2, 3, 4, 6, 8) for sides in (4, 8)),
)
PIXEL_BUDGET = 0.25
AREA_BUDGET = 0.05


def specimen(base, product, plans):
    tree = plans[product["plant_id"]]
    recipe = json.loads((base.parent / tree["recipe"]).read_text())
    index = int(Path(product["mesh"]).stem.rsplit("-", 1)[1])
    return Needle(**recipe["needles"][index]), tree["species"]["latin"]


def compare(shape, origins, scales, camera, guard_area):
    reference = projected_mesh_bound(shape, 8, 8, origins, scales, camera)
    reference_areas = projected_areas(shape.mesh(8, 8)) if guard_area else None
    candidates = CANDIDATES if guard_area else UNIFORM_CANDIDATES
    records = []
    timings = {candidate: [] for candidate in candidates}
    for repeat in range(5):
        for segments, sides, sampling in (
            candidates if repeat % 2 == 0 else reversed(candidates)
        ):
            began = time.perf_counter()
            shape.mesh(segments, sides, axial_sampling=sampling)
            timings[(segments, sides, sampling)].append(
                1000 * (time.perf_counter() - began)
            )
    for segments, sides, sampling in candidates:
        mesh = shape.mesh(segments, sides, axial_sampling=sampling)
        check_mesh(mesh)
        predicted = projected_mesh_bound(
            shape, segments, sides, origins, scales, camera, sampling
        )
        baseline = (segments, sides, sampling) == (8, 8, "uniform")
        pair_bound = 0.0 if baseline else predicted + reference
        samples = independent_interpolation_samples(shape, segments, sides, sampling)
        area_error = (
            projected_areas(mesh) / reference_areas - 1
            if guard_area
            else np.array([0.0])
        )
        times = timings[(segments, sides, sampling)]
        records.append(
            {
                "segments": segments,
                "sides": sides,
                "axial_sampling": sampling,
                "triangles": len(mesh["faces"]),
                "analytic_surface_error_px": predicted,
                "reference_pair_error_px": pair_bound,
                "area_relative_min": float(area_error.min()) if guard_area else None,
                "area_relative_max": float(area_error.max()) if guard_area else None,
                "eligible": bool(
                    pair_bound <= PIXEL_BUDGET
                    and np.max(abs(area_error)) <= AREA_BUDGET
                ),
                "mesh_samples_ms": times,
                "median_mesh_ms": float(np.median(times)),
                **samples,
            }
        )
    eligible = [r for r in records if r["eligible"]]
    chosen = min(eligible, key=lambda r: (r["triangles"], r["reference_pair_error_px"]))
    return records, chosen


def alternative_scene(source, camera, output, guard_area):
    result, measurements = [], []
    plans = json.loads((BASE.parent / source["plant_recipe"]).read_text())["plants"]
    triangles, reference_triangles, objects = 0, 0, 0
    for product in source["products"]:
        original = dict(product)
        for field in ("mesh", "placements", "bark_material", "needle_material"):
            if field in original:
                original[field] = str((BASE.parent / original[field]).resolve())
        with np.load(original["mesh"]) as mesh:
            old_triangles = len(mesh["faces"]) * original["instances"]
        reference_triangles += old_triangles
        objects += original["instances"]
        if "needle_material" in original:
            shape, latin = specimen(BASE, product, plans)
            control = shape.mesh(8, 8)
            with np.load(original["mesh"]) as stored:
                if set(stored.files) != set(control) or any(
                    stored[key].dtype != control[key].dtype
                    or stored[key].tobytes() != control[key].tobytes()
                    for key in control
                ):
                    raise AssertionError("cached control needle differs from recipe")
            with np.load(original["placements"]) as poses:
                records, chosen = compare(
                    shape, poses["origins"], poses["scales"], camera, guard_area
                )
            new_mesh = shape.mesh(
                chosen["segments"],
                chosen["sides"],
                axial_sampling=chosen["axial_sampling"],
            )
            destination = output / Path(original["mesh"]).name.replace(
                "-whole-", "-selected-"
            )
            np.savez_compressed(destination, **new_mesh)
            original["mesh"] = str(destination)
            old_triangles = len(new_mesh["faces"]) * original["instances"]
            measurements.append(
                {
                    "latin": latin,
                    "prototype": Path(product["mesh"]).stem,
                    "count": product["instances"],
                    "input_sha256": {
                        original["placements"]: hashlib.sha256(
                            Path(original["placements"]).read_bytes()
                        ).hexdigest(),
                        str(BASE.parent / product["mesh"]): hashlib.sha256(
                            (BASE.parent / product["mesh"]).read_bytes()
                        ).hexdigest(),
                    },
                    "candidates": records,
                    "chosen": chosen,
                }
            )
            print(
                Path(product["mesh"]).stem,
                chosen["segments"],
                chosen["sides"],
                chosen["axial_sampling"],
                flush=True,
            )
        triangles += old_triangles
        result.append(original)
    return (
        result,
        measurements,
        {
            "reference_triangles": reference_triangles,
            "selected_triangles": triangles,
            "same_instances": objects,
        },
    )


def numeric_counterexamples():
    cases = []
    shapes = (
        Needle(),
        Needle(section="rhombus", bend=-0.8, twist=2),
        Needle(section="sector", sector_count=3, bend=-0.6, twist=-1.7),
        Needle(aspect=0.125, bend=0, twist=0, taper_start=0.875),
        Needle(section="sector", sector_count=8, taper_start=0.25, bend=0.7, twist=2.3),
    )
    for shape in shapes:
        for segments, sides, sampling in (
            (n, k, s)
            for n, k in ((2, 4), (4, 8), (8, 16))
            for s in ("uniform", "profile")
        ):
            for taper in (0.25, 0.5, 0.75, 0.875):
                variant = replace(shape, taper_start=taper)
                result = independent_interpolation_samples(
                    variant, segments, sides, sampling
                )
                cases.append(
                    {
                        "shape": variant.recipe(),
                        "segments": segments,
                        "sides": sides,
                        "axial_sampling": sampling,
                        **result,
                    }
                )
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--position-only", action="store_true")
    args = parser.parse_args()
    guard_area = not args.position_only
    output = (OUTPUT / ("position-area" if guard_area else "position-only")).resolve()
    output.mkdir(parents=True, exist_ok=True)
    base_data = json.loads(BASE.read_text())
    camera = Camera(
        tuple(base_data["camera"]),
        tuple(base_data["target"]),
        width=base_data["resolution"][0],
        height=base_data["resolution"][1],
        fov_y=base_data["fov_y"],
    )
    began = time.perf_counter()
    products, cases, totals = alternative_scene(base_data, camera, output, guard_area)
    scene = dict(base_data)
    scene["products"] = products
    for field in ("plant_recipe", "tables"):
        scene[field] = str((BASE.parent / scene[field]).resolve())
    scene.update(
        geometry_lod="closed needles selected by position envelope"
        + (" and sampled projected areas" if guard_area else " only"),
        source_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        source_modified=True,
        needle_projection_budget_px=PIXEL_BUDGET,
        sampled_area_budget_relative=AREA_BUDGET if guard_area else None,
        material_and_shadow_error_proven=False,
        lod_image_error_proven=False,
    )
    (output / "selected.json").write_text(json.dumps(scene, indent=2) + "\n")
    report = {
        "cases": cases,
        "totals": totals,
        "pixel_budget": PIXEL_BUDGET,
        "area_budget_relative": AREA_BUDGET if guard_area else None,
        "area_views": 70 if guard_area else 0,
        "area_snap_grid_m": SNAP_GRID_M if guard_area else None,
        "preparation_s": time.perf_counter() - began,
        "counterexamples": numeric_counterexamples(),
        "control_scene": str(BASE),
        "control_sha256": hashlib.sha256(BASE.read_bytes()).hexdigest(),
        "implementation_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("needle_lod_bounds.py"),
                Path(__file__).with_name("needle_projected_area.py"),
                Path(__file__).with_name("demand.py"),
                *(
                    Path(needle_surface.__file__).with_name(n)
                    for n in (
                        "needle_surface.py",
                        "needle_sampling.py",
                        "needle_section.py",
                        "curve_frame.py",
                    )
                ),
            )
        },
        "scope": "position bound and optional sampled areas; equal render/shading/shadows still required",
    }
    (output / "needle-lod-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(totals, indent=2))
    for case in cases:
        print(
            case["prototype"],
            case["chosen"]["segments"],
            case["chosen"]["sides"],
            case["chosen"]["axial_sampling"],
            case["chosen"]["reference_pair_error_px"],
        )


if __name__ == "__main__":
    main()
