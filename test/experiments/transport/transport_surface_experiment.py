"""Offline source-bound face-family integrals and unchanged-body material impression."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from transport_surface_areas import centroid_areas, family_areas
from transport_surface_families import FAMILIES, MATERIALS, family_domains
from transport_surface_inputs import inputs

HERE = Path(__file__).resolve().parent
PALETTE = HERE.parents[2] / "src/assets/world/ground-materials.json"


def albedos(camera):
    document = json.loads(PALETTE.read_text())
    values = {m["name"]: m["albedo"] for m in document["classes"]}
    rgb = np.array(
        [camera["colours"][0][:3] if key is None else values[key] for key in MATERIALS],
        float,
    )
    return rgb, {
        "path": str(PALETTE),
        "sha256": digest(PALETTE),
        "materials": list(MATERIALS),
        "albedos_linear_rgb": rgb.tolist(),
        "scope": "Existing linear values only; family-to-material choices are procedural, not supplied asphalt/stone identities. Existing roughness/metallic/light unchanged.",
    }


def presentation(output, baseline, arrays, selected, areas, ground_triangles):
    source = json.loads((baseline / "after-scene.json").read_text())
    camera = {
        k: source[k]
        for k in [
            "projection",
            "profile",
            "samples",
            "colours",
            "eye",
            "target",
            "span_m",
            "camera_basis",
            "fov_y_degrees",
        ]
        if k in source
    }
    rgb, material = albedos(camera)
    area = shapely.area(shapely.polygons(ground_triangles))
    palette = np.asarray(camera["colours"], np.float32)
    colour = palette[arrays["materials"]].copy()
    colour[selected, :3] = (areas @ rgb / area[:, None]).astype(np.float32)
    path = output / "after-body.npz"
    np.savez_compressed(path, **arrays, face_base_colour_linear_rgba=colour)
    rows = []
    for name, mesh in [("before", baseline / "after-body.npz"), ("after", path)]:
        row = dict(
            camera,
            id="Wien-surface-families-" + name,
            mesh=str(mesh),
            mesh_sha256=digest(mesh),
            output=str(output / (name + ".png")),
            source_array_payload_bytes=sum(a.nbytes for a in arrays.values())
            + (colour.nbytes if name == "after" else 0),
        )
        (output / (name + "-scene.json")).write_text(json.dumps(row, indent=2) + "\n")
        rows.append(row)
    high = dict(
        rows[1],
        id="Wien-surface-families-after-1080",
        profile=[1920, 1080],
        output=str(output / "after-1080.png"),
    )
    (output / "after-1080-scene.json").write_text(json.dumps(high, indent=2) + "\n")
    return (
        rows,
        material,
        {
            "owned_face_colour_bytes": colour.nbytes,
            "known_base_array_payload_bytes": sum(a.nbytes for a in arrays.values()),
        },
    )


def build(args):
    began = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
        ).strip()
    ):
        raise ValueError("fresh output and clean frozen source required")
    args.output.mkdir(parents=True)
    ground, road, selected, records, arrays, binding = inputs(
        args.network, args.baseline
    )
    domains, domain_report = family_domains(records)
    metadata = [{k: v for k, v in r.items() if k != "shape"} for r in records]
    (args.output / "sources.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (args.output / "families.json").write_text(
        json.dumps(
            {
                "names": FAMILIES,
                "domains_wkb_hex": [shapely.to_wkb(d, hex=True) for d in domains],
                "domains": domain_report,
            },
            indent=2,
        )
        + "\n"
    )
    triangles = ground.vertices[ground.triangles[road], :2]
    methods, results = {}, {}
    for method in ["whole", "cells"]:
        amounts, reasons, work = family_areas(
            triangles, domains, method=method, seconds=60
        )
        path = args.output / ("areas-" + method + ".npz")
        np.savez_compressed(
            path, areas_xy_m2=amounts, reasons=reasons, body_face_ids=selected
        )
        methods[method] = dict(
            work, sha256=digest(path), known_area_xy_m2=amounts.sum(axis=0).tolist()
        )
        results[method] = amounts
        print(json.dumps({"method": method, **work}), flush=True)
    comparable = np.max(abs(results["whole"] - results["cells"]))
    exhausted = any(
        methods[m]["reason_counts"][str(i)] for m in ["whole", "cells"] for i in (1, 2)
    )
    if exhausted or comparable > 1e-7:
        raise ValueError(
            "actual family alternatives incomplete or geometrically different"
        )
    started = time.perf_counter()
    sampled = centroid_areas(triangles, domains)
    methods["centroid"] = {
        "seconds": time.perf_counter() - started,
        "l1_reclassified_xy_m2": float(abs(sampled - results["cells"]).sum() / 2),
    }
    np.savez_compressed(
        args.output / "areas-centroid.npz", areas_xy_m2=sampled, body_face_ids=selected
    )
    methods["centroid"]["sha256"] = digest(args.output / "areas-centroid.npz")
    rows, material, copies = presentation(
        args.output, args.baseline, arrays, selected, results["cells"], triangles
    )
    amounts = results["cells"]
    area = amounts.sum(axis=1)
    mixed = (
        np.count_nonzero(amounts > np.maximum(1e-10, area[:, None] * 1e-9), axis=1) > 1
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "bindings": binding,
        "families": list(FAMILIES),
        "methods": methods,
        "maximum_method_area_difference_m2": float(comparable),
        "mixed_faces": int(mixed.sum()),
        "mixed_xy_area_m2": float(area[mixed].sum()),
        "faces": len(triangles),
        "source_corridors": len(records),
        "sources_sha256": digest(args.output / "sources.json"),
        "families_sha256": digest(args.output / "families.json"),
        "material": material,
        "copies": copies,
        "render_inputs": rows,
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Exact projected family integrals on unchanged actual lower facets. Their constant linear albedo means are a facet approximation, not exact spatial material boundaries or pixel filtering. Upper/deep sources do not label ground. Source usage/traffic/positions/topology/normals unchanged. No material/lighting/native API or device acceptance.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in ["commit", "faces", "mixed_faces", "seconds", "peak_rss_kib"]
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["network", "baseline", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    build(args)
