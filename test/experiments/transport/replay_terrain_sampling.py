"""Compare exact original DEM sampling and complete actual contact products."""

import argparse
import gc
import hashlib
import importlib.util
import json
import os
import platform
import subprocess
import tempfile
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely
from ground_contact import build_contact
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_contact_plot import draw
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.places.height import Elevation
from outshine_ref.transport.canonical_ports._geodesy import (
    ground_geographic_at,
    positions_in_frame,
)
from rail_ground_bed import construct
from rail_ground_checks import audit
from rail_ground_inputs import FIXTURE, load
from rail_ground_profile import bind_profile
from replay_canonical_bridge_ports import allocations
from replay_rail_ground_bed import arrays

BASELINE = "b9d9f5aaec192da712ec83776e04f46a9408eec5"
HEIGHT = "lib/src/outshine_ref/places/height.py"


def original_class(output):
    body = subprocess.check_output(["git", "show", f"{BASELINE}:{HEIGHT}"])
    path = output / "original_height.py"
    path.write_bytes(body)
    spec = importlib.util.spec_from_file_location(
        "outshine_ref.places._frozen_sampling_reference", path
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.Elevation, hashlib.sha256(body).hexdigest()


def digest(fields):
    value = hashlib.sha256()
    for key, array in sorted(fields.items()):
        value.update(key.encode())
        value.update(str((array.dtype, array.shape)).encode())
        value.update(array.tobytes())
    return value.hexdigest()


def target(sampler, anchor):
    def sample(points):
        geographic = ground_geographic_at(anchor, points)
        heights = sampler.sample(geographic[:, 1], geographic[:, 0])
        return positions_in_frame(anchor, geographic, heights)[:, 2]

    return sample


def soil_error(result, sample_z):
    contact, bed = result["terrain"], result["bed"]
    x0, y0, x1, y1 = contact["domain"].bounds
    x, y = np.meshgrid(np.linspace(x0, x1, 97), np.linspace(y0, y1, 97))
    queries = np.c_[x.ravel(), y.ravel()]
    domain = shapely.union_all(shapely.polygons(bed.vertices[bed.triangles, :2]))
    queries = queries[~shapely.intersects_xy(domain, queries[:, 0], queries[:, 1])]
    truth, report = sample_z(queries), {}
    for name, mesh in (("coarse", contact["coarse_mesh"]), ("fine", contact["mesh"])):
        sampled = SurfaceSampler(mesh).sample(queries)
        if any(sampled.reasons):
            raise ValueError("Independent soil evaluation lost coverage")
        error = abs(truth - sampled.points_enu_m[:, 2])
        report[name] = {
            "maximum_m": float(error.max()),
            "p95_m": float(np.quantile(error, 0.95)),
            "rms_m": float(np.sqrt(np.mean(error**2))),
        }
    return {
        "positions": len(queries),
        "query_sha256": hashlib.sha256(queries.tobytes()).hexdigest(),
        "errors": report,
        "scope": "Independent finite soil-only DEM error; not a continuous bound. Fine/coarse deformation is reported separately by refinement.",
    }


def compare_street(samplers):
    context = load_context()
    products, reference = {}, None
    for name, sampler in samplers.items():
        sample_z = target(sampler, context.anchor)
        np.testing.assert_array_equal(
            sample_z(context.road.vertices[:, :2]), context.road_dem_z
        )
        contact = build_contact(context.road, context.road_dem_z, sample_z)
        if contact["status"] != "candidate":
            raise ValueError("Original street contact became pending")
        fields = {
            "vertices": contact["mesh"].vertices,
            "triangles": contact["mesh"].triangles,
            "coarse_vertices": contact["coarse_mesh"].vertices,
            "coarse_triangles": contact["coarse_mesh"].triangles,
            "bed_map": contact["road_vertex_map"],
            "prior_z": contact["prior_z"],
            "roots": contact["refinement_roots"],
            "outer": contact["outer_vertex_ids"],
        }
        signature = digest(fields)
        reference = signature if reference is None else reference
        if signature != reference:
            raise ValueError("Original whole street contact changed")
        products[name] = {"sha256": signature, "fields": len(fields)} | qualify_contact(
            context.road, contact
        )
    return products


def mixed_queries():
    u, v = np.meshgrid(np.linspace(0.125, 0.875, 64), np.linspace(0.125, 0.875, 64))
    tile = np.tile((5678, 5679), u.size // 2)
    world_x, world_y = (8937 + u.ravel()) / 2**14, (tile + v.ravel()) / 2**14
    return np.rad2deg(
        np.arctan(np.sinh(np.pi * (1 - 2 * world_y)))
    ), world_x * 360 - 180


def replay(args):
    repository = Path(__file__).resolve().parents[3]
    temporary = Path(tempfile.gettempdir()).resolve()
    output = args.output.resolve()
    if (
        output.is_relative_to(repository)
        or not output.is_relative_to(temporary)
        or output == temporary
    ):
        raise ValueError("Private sampling evidence belongs in system temp")
    producers = [HEIGHT, "test/experiments/transport"]
    if subprocess.check_output(["git", "status", "--porcelain", "--", *producers]):
        raise ValueError("Freeze sampling producers before recording costs")
    args.output.mkdir(parents=True, exist_ok=False)
    previous, original_hash = original_class(args.output)
    index, cases, _terrain, budget, metadata, raw, manifest = load()
    case = next(c for c in cases if len(c["selected"]) == 7)
    profile = bind_profile(index, case["selected"], metadata["source_binding"], raw)
    receipts = []
    for key in ("existing_elevation", "additional_elevation"):
        row = dict(manifest[key])
        row["path"] = str((FIXTURE / row["path"]).resolve())
        receipts.append(row)
    samplers = {"sorted_original": previous(receipts), "one_tile": Elevation(receipts)}
    for sampler in samplers.values():
        for row in receipts:
            sampler.raster(tuple(row["tile"]))

    def generate(sampler):
        return construct(
            index, profile, lambda p: sampler.sample(p[:, 1], p[:, 0]), budget
        )

    products = {name: generate(sampler) for name, sampler in samplers.items()}
    if any(result["state"] != "constructed" for result in products.values()):
        raise ValueError("Actual rail contact became pending")
    expected = digest(arrays(products["sorted_original"]))
    for result in products.values():
        if digest(arrays(result)) != expected:
            raise ValueError("Original full rail product changed")
        audit(result, index, profile)
    timed = measure(samplers, generate, expected, args.output)
    new = products["one_tile"]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "baseline": BASELINE,
        "baseline_height_sha256": original_hash,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "load_average": os.getloadavg(),
        "input_manifest": manifest,
        "methods": timed,
        "full_rail_sha256": expected,
        "exact_rail_fields": len(arrays(new)),
        "rail_work": new["work"],
        "rail_contact": audit(new, index, profile),
        "street": compare_street(samplers),
        "soil_error": soil_error(new, target(samplers["one_tile"], new["anchor"])),
        "mixed_tiles": measure_mixed(samplers),
        "scope": "Same complete warm rail product with original/missing source facts and fixed whole bed/rim/grade budgets. Input preparation, raster decode, audits/exports/plot and trace processing outside timings. Pure one-tile grouping shortcut; no query memoization, native/device/p99/UMA claim.",
    }
    np.savez_compressed(args.output / "rail.npz", **arrays(new))
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.plot:
        draw(
            SimpleNamespace(road=new["bed"], report={"terminal_values": []}),
            new["terrain"],
            args.output,
            title="Identical actual seven-source rail bed and terrain\nSingle-tile sampler grouping shortcut only; original profiles/Unknown and all contact limits unchanged",
        )
    print(json.dumps({name: value["median_cpu_s"] for name, value in timed.items()}))


def measure(samplers, generate, expected, output):
    import memray

    report = {name: {"runs": []} for name in samplers}
    for turn in range(7):
        for name in list(samplers) if turn % 2 == 0 else list(samplers)[::-1]:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            product = generate(samplers[name])
            report[name]["runs"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if digest(arrays(product)) != expected:
                raise ValueError("Timed rail product changed")
            del product
    for name, sampler in samplers.items():
        trace = output / f"{name}.bin"
        gc.collect()
        with memray.Tracker(
            str(trace), trace_python_allocators=True, native_traces=False
        ):
            product = generate(sampler)
        if digest(arrays(product)) != expected:
            raise ValueError("Traced rail product changed")
        report[name].update(
            median_cpu_s=float(np.median([r["cpu_s"] for r in report[name]["runs"]])),
            allocations=allocations(trace)
            | {
                "scope": "Warm full construct only; existing sources/decoded DEM excluded."
            },
            known_mesh_copy_lower_bound_bytes=sum(
                m.vertices.nbytes + m.triangles.nbytes
                for m in (
                    product["bed"],
                    product["terrain"]["coarse_mesh"],
                    product["terrain"]["mesh"],
                )
            ),
            total_internal_copied_bytes=None,
        )
    return report


def measure_mixed(samplers):
    latitude, longitude = mixed_queries()
    outputs = {
        name: sampler.sample_with_zoom(latitude, longitude)
        for name, sampler in samplers.items()
    }
    for i in (0, 1):
        np.testing.assert_array_equal(
            outputs["sorted_original"][i], outputs["one_tile"][i]
        )
    runs = {name: [] for name in samplers}
    for turn in range(7):
        for name in list(samplers) if turn % 2 == 0 else list(samplers)[::-1]:
            cpu = time.process_time()
            value = samplers[name].sample_with_zoom(latitude, longitude)
            runs[name].append(time.process_time() - cpu)
            for i in (0, 1):
                np.testing.assert_array_equal(value[i], outputs[name][i])
    return {
        "positions": len(latitude),
        "same_height_and_zoom": True,
        "runs_cpu_s": runs,
        "median_cpu_s": {k: float(np.median(v)) for k, v in runs.items()},
        "scope": "Actual two-tile interleaved DEM control; shortcut refuses nonuniform cells and uses the original grouping.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--plot", action="store_true")
    replay(parser.parse_args())
