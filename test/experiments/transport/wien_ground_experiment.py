"""Compare fixed old contacts with joint terrain replanning in bounded fresh workers."""

import argparse
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
import shapely
from bounded_ground_fit import fit
from camera_height_inputs import digest
from ground_surface_showcase import body, combined
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, fit_surface_heights
from surface_solid_checks import audit_closed
from transport_gallery_wien import render
from tunnel_portal_experiment import source_usage
from tunnel_portal_showcase import assembled
from wien_ground_checks import check
from wien_ground_inputs import prepare


def worker(method, source, output):
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (20, 20))
    with np.load(source) as data:
        a = dict(data)
    bounds = tuple(
        a[key] for key in ("bound_columns", "bound_coefficients", "bound_limits")
    )
    samples, z, report = [], None, {}
    for _ in range(5):
        began = time.perf_counter()
        if method == "fixed":
            pins = dict(
                zip(map(int, a["old_vertex_map"]), map(float, a["old_heights"]))
            )
            result = fit_surface_heights(
                Mesh(np.c_[a["xy"], a["targets"]], a["faces"]),
                a["frames"],
                fixed=pins,
                budget=HeightBudget(time_limit_s=8),
            )
            z = None if result.mesh is None else result.mesh.vertices[:, 2]
            report = dict(result.report)
        else:
            weights = a["weights"] if method == "weighted" else np.ones(len(a["xy"]))
            z, report = fit(
                a["xy"], a["faces"], a["targets"], a["frames"], bounds, weights
            )
            if not report["valid"]:
                raise ValueError("joint fit repetition absent: " + str(report))
        samples.append((time.perf_counter() - began) * 1000)
    row = {
        "method": method,
        "input_sha256": digest(source),
        "report": report,
        "samples_ms": samples,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "limits": {
            "address_space_bytes": 2 * 1024**3,
            "cpu_seconds": 20,
            "parent_timeout_s": 25,
        },
    }
    if z is not None:
        np.savez_compressed(output / (method + ".npz"), heights=z)
        row["mesh_sha256"] = digest(output / (method + ".npz"))
        row["area_weighted_dem_rms_m"] = float(
            np.sqrt(np.average((z - a["targets"]) ** 2, weights=a["weights"]))
        )
    (output / (method + ".json")).write_text(json.dumps(row, indent=2) + "\n")


def contradiction(arrays, domain):
    xy = arrays["xy"][arrays["old_vertex_map"]]
    z = arrays["old_heights"]
    if len(xy) > 2048:
        raise ValueError("fixed-contact diagnostic pair budget")
    distance = np.linalg.norm(xy[:, None] - xy[None, :], axis=2)
    change = abs(z[:, None] - z[None, :])
    slope = np.divide(change, distance, out=np.zeros_like(change), where=distance > 0)
    first, second = np.unravel_index(slope.argmax(), slope.shape)
    segment = shapely.LineString(xy[[first, second]])
    if not domain.covers(segment) or slope[first, second] <= 2 / 3:
        raise ValueError("no independent fixed-contact contradiction")
    return {
        "old_vertex_ids": [int(first), int(second)],
        "xy_m": xy[[first, second]].tolist(),
        "heights_m": z[[first, second]].tolist(),
        "distance_m": float(distance[first, second]),
        "height_difference_m": float(change[first, second]),
        "required_grade": float(slope[first, second]),
        "complete_segment_inside_domain": True,
    }


def generate(reference, sources, output, *, images=True):
    began = time.perf_counter()
    if output.exists() and any(output.iterdir()):
        raise ValueError("fresh empty experiment output required")
    output.mkdir(parents=True, exist_ok=True)
    arrays, meta, fixture, previous, model = prepare(reference, sources)
    source = output / "input.npz"
    np.savez_compressed(source, **arrays)
    domain = shapely.from_geojson(meta["domain_geojson"])
    meta["contradiction"] = contradiction(arrays, domain)
    methods = {}
    for method in ("fixed", "unweighted", "weighted"):
        subprocess.run(
            [
                sys.executable,
                "-c",
                "import subprocess,sys; subprocess.run(sys.argv[1:],check=True,timeout=25)",
                sys.executable,
                __file__,
                "--worker",
                method,
                "--input",
                str(source),
                "--output",
                str(output),
            ],
            check=True,
            timeout=30,
        )
        row = json.loads((output / (method + ".json")).read_text())
        if row["input_sha256"] != digest(source):
            raise ValueError("worker height input changed")
        if "mesh_sha256" in row:
            path = output / (method + ".npz")
            if digest(path) != row["mesh_sha256"]:
                raise ValueError("worker height payload changed")
            with np.load(path) as data:
                heights = data["heights"]
            row["independent"] = check(
                arrays, heights, model, previous, meta["old_recipe"], domain
            )
        methods[method] = row
    if methods["fixed"]["report"]["valid"]:
        raise ValueError("incompatible old contacts unexpectedly accepted")
    if not all(methods[m]["report"]["valid"] for m in ("unweighted", "weighted")):
        raise ValueError("joint ground comparison absent")
    if (
        methods["weighted"]["area_weighted_dem_rms_m"]
        > methods["unweighted"]["area_weighted_dem_rms_m"] + 1e-7
    ):
        raise ValueError("weighted candidate loses the declared area objective")
    with np.load(output / "unweighted.npz") as data:
        mesh = Mesh(np.c_[arrays["xy"], data["heights"]], arrays["faces"])
    old_body, _slots = assembled(model, previous)
    structure, slots = assembled(
        {k: v for k, v in model.items() if k != "ground"}, previous
    )
    colours = np.r_[
        np.zeros(int(arrays["road_count"]), int),
        np.full(len(mesh.triangles) - int(arrays["road_count"]), 5),
    ]
    solid, slots = combined([(structure, slots), body(mesh, colours, 0.25)])
    np.savez_compressed(
        output / "solid.npz",
        vertices=solid.vertices,
        triangles=solid.triangles,
        materials=slots,
    )
    meta.update(
        input_sha256=digest(source),
        methods=methods,
        selected_method="unweighted",
        solid_sha256=digest(output / "solid.npz"),
        closed_volume_m3=audit_closed(solid),
        source_usage=source_usage(fixture, meta["old_recipe"]),
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        dirty=bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "lib",
                    str(Path(__file__).parent),
                ],
                text=True,
            ).strip()
        ),
        scope="Joint C0 terrain/road shell only. Upper C1/portal products unchanged. Lower roads have new heights; former contact products are not replaced. No new piers, earth volume, retaining portal walls, full tile traffic, hardware or native integration.",
    )
    if images:
        for name, geometry, materials in (
            ("before", old_body, _slots),
            ("after", solid, slots),
        ):
            target = output / name
            target.mkdir()
            meta[name] = render(
                "Wien",
                geometry,
                materials,
                target,
                [1280, 720],
                (-0.3, -1, 0.45),
                camera_vertices=old_body.vertices,
            )
    meta["seconds"] = time.perf_counter() - began
    (output / "comparison.json").write_text(json.dumps(meta, indent=2) + "\n")
    print(
        json.dumps(
            {
                "output": str(output),
                "seconds": meta["seconds"],
                "methods": {m: r["report"]["valid"] for m, r in methods.items()},
            }
        ),
        flush=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-root", type=Path)
    parser.add_argument("--dem-sources", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--worker", choices=("fixed", "unweighted", "weighted"))
    parser.add_argument("--input", type=Path)
    parser.add_argument("--no-images", action="store_true")
    args = parser.parse_args()
    if args.worker:
        worker(args.worker, args.input, args.output)
    elif args.reference_root is None or args.dem_sources is None:
        parser.error("explicit pinned reference root and cached DEM sources required")
    else:
        generate(
            args.reference_root,
            args.dem_sources,
            args.output,
            images=not args.no_images,
        )


if __name__ == "__main__":
    main()
