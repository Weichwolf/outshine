"""Same-source curved-alpha coverage and signed raw-normal comparison on real hosts."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from cloud_experiment import view_bounds
from connected_catalog import ConnectedCatalog
from foliage_selection import directions
from group_cloud import greedy_cloud
from leaf_patch_catalog import LeafPatchCatalog
from leaf_sheet import average_blocks
from shoot_plan import normal_frame
from spatial_group_source import load_source
from surface_fragments import resolve, surface_hits


def chart_surfaces(charts, source, *, independent=False):
    return tuple(
        {
            "mesh": c.mesh,
            "colour": c.colour,
            "normal": c.normal,
            "basis": c.frame,
            "owner": np.min(source["ids"][c.members]),
            "component": i % 4 if independent else i,
        }
        for i, c in enumerate(charts)
    )


def project(surfaces, camera, bounds, pixels, lights):
    packets = []
    count = 0
    for surface in surfaces:
        packet = surface_hits(camera=camera, bounds=bounds, size=pixels * 8, **surface)
        count += len(packet.pixel)
        if count > 1048576:
            raise ValueError("retained group fragment capacity exceeded")
        packets.append(packet)
    image = average_blocks(resolve(packets, lights, pixels * 8), 8)
    return image, count


def metrics(reference, reduced):
    union = float(np.maximum(reference[..., 0], reduced[..., 0]).sum())
    diff = abs(reference - reduced)
    return {
        "coverage_l1_over_union": float(diff[..., 0].sum() / max(union, 1e-12)),
        "signed_response_l1_over_union": (
            diff[..., 1:].sum(axis=(0, 1)) / max(union, 1e-12)
        ).tolist(),
        "maximum_unit_response_pixel_error": float(diff[..., 1:].max()),
    }


def host_case(source, catalogs, independent, node, output):
    output.mkdir()
    before = time.perf_counter()
    exact = source.geometry(node)
    reference_s = time.perf_counter() - before
    meshes = {
        "reference": ({"mesh": exact, "owner": exact["ids"][exact["organ_faces"]]},)
    }
    before = time.perf_counter()
    meshes["spatial"] = chart_surfaces(greedy_cloud(exact, size=16), exact)
    times = {
        "reference_build_s": reference_s,
        "spatial_build_s": time.perf_counter() - before,
    }
    g = independent.group(node)
    meshes["independent4"] = chart_surfaces(
        independent.charts(g), exact, independent=True
    )
    for name, catalog in catalogs.items():
        before = time.perf_counter()
        group = catalog.group(node)
        times[name + "_build_group_s"] = time.perf_counter() - before
        np.testing.assert_array_equal(group["poses"]["ids"], exact["ids"])
        np.testing.assert_array_equal(
            group["physical_leaf_area_m2"], exact["physical_leaf_area_m2"]
        )
        np.testing.assert_array_equal(
            group["poses"]["variants"], exact["leaf_variants"]
        )
        meshes[name] = catalog.surfaces(group)
        np.savez_compressed(
            output / (name + "-poses.npz"),
            **group["poses"],
            physical_leaf_area_m2=group["physical_leaf_area_m2"],
        )
    all_points = np.vstack([s["mesh"]["vertices"] for v in meshes.values() for s in v])
    lights = directions(6)
    rows = []
    images = {}
    for query, direction in enumerate(directions(24)):
        basis = normal_frame(direction, (0, 0, 1))
        camera = np.column_stack((basis[:, 0], basis[:, 2], basis[:, 1]))
        bounds = view_bounds(all_points, direction)
        for pixels in (4, 8, 16):
            reference, source_fragments = project(
                meshes["reference"], camera, bounds, pixels, lights
            )
            for name, surfaces in meshes.items():
                if name == "reference":
                    continue
                image, fragments = project(surfaces, camera, bounds, pixels, lights)
                rows.append(
                    {
                        "query": query,
                        "pixels": pixels,
                        "method": name,
                        "reference_fragments": source_fragments,
                        "candidate_fragments": fragments,
                        **metrics(reference, image),
                    }
                )
                if query < 2 and pixels == 8:
                    images[f"{name}-{query}-reference"] = reference
                    images[f"{name}-{query}-image"] = image
    np.savez_compressed(output / "images.npz", **images)
    np.savez_compressed(
        output / "source.npz",
        **{k: a for k, a in exact.items() if isinstance(a, np.ndarray)},
    )
    return {
        "host_node": node,
        "leaves": len(exact["ids"]),
        "physical_area_m2": float(exact["physical_leaf_area_m2"].sum()),
        "triangles": {
            n: sum(len(s["mesh"]["faces"]) for s in v) for n, v in meshes.items()
        },
        "build_times": times,
        "comparisons": rows,
    }


def study(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    source = load_source(inputs)
    catalogs = {
        f"connected{size}": ConnectedCatalog(load_source(inputs), size=size)
        for size in (16, 32)
    }
    independent = LeafPatchCatalog(load_source(inputs), 4, 16)
    summary = source.summary()
    results = []
    with patch(
        "crown_shoots.CrownShoots.tables",
        side_effect=AssertionError("whole pose table"),
    ):
        for node in source.crown.hosts[
            np.linspace(0, len(source.crown.hosts) - 1, 4, dtype=int)
        ]:
            results.append(
                host_case(
                    source, catalogs, independent, int(node), output / f"host-{node}"
                )
            )
            print(
                json.dumps(
                    {k: v for k, v in results[-1].items() if k != "comparisons"}
                ),
                flush=True,
            )
    for name, catalog in catalogs.items():
        folder = output / name
        folder.mkdir()
        for variant, p in catalog.prototypes.items():
            np.savez_compressed(
                folder / f"prototype-{variant}.npz",
                **p["mesh"],
                colour=p["colour"],
                normal=p["normal"],
            )
            (folder / f"prototype-{variant}.json").write_text(
                json.dumps(
                    {
                        k: v
                        for k, v in p.items()
                        if k not in ("mesh", "colour", "normal")
                    },
                    indent=2,
                )
                + "\n"
            )
    result = {
        "source": summary,
        "catalogs": {n: c.summary() for n, c in catalogs.items()},
        "independent4": independent.summary(),
        "cases": results,
        "query_directions": directions(24).tolist(),
        "unit_lights": directions(6).tolist(),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "wall_s": time.perf_counter() - start,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Raw linear generated float arrays for every method, no old8-bit alpha quantization. Exact original poses/IDs/physical area; all returned curved alpha layers, source opaque normals and unit front/back cosine only. Not calibrated R/T, neighbour lighting, BRDF, wind/transition/forest/native/device admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "cases"}), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("inputs", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    study(a.inputs, a.output)
