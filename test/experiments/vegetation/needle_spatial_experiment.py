"""Same-source spatial patch/remaining geometry tradeoffs over independent views."""

import hashlib
import json
import subprocess
import time
from pathlib import Path

import matplotlib
import numpy as np
from foliage_selection import directions
from needle_group_experiment import BASE, error_summary
from needle_group_reprojection import compare_images, reproject
from needle_group_source import source_geometry
from needle_group_views import load_views, projection, render_view
from needle_spatial_cache import save_product
from needle_spatial_planes import plane_patches
from needle_spatial_product import bake_patches, spatial_view

HERE = Path(__file__).resolve().parent
OUTPUT = Path("build/experiments/vegetation/needle-spatial")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def diagnostic(species, images):
    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    fig, axes = plt.subplots(1, 4, figsize=(16, 9))
    labels = (
        "Closed 3D reference",
        "One card / 64 views",
        "Spatial patches / 0.25 mm",
        "Spatial patches / 0.50 mm",
    )
    for axis, image, title in zip(axes, images, labels, strict=True):
        axis.imshow(image["rgba"][..., 3], origin="lower", cmap="gray", vmin=0, vmax=1)
        axis.set_title(title)
        axis.set_axis_off()
    fig.tight_layout()
    fig.savefig(OUTPUT / f"{species}-comparison.png", dpi=160)
    plt.close(fig)


def compare(species):
    source = source_geometry(BASE / species)
    validation, sphere = directions(17), directions(64)
    references = [render_view(source, d, 64, samples=8) for d in validation]
    control_path = Path("build/experiments/vegetation/needle-groups")
    control = next(
        r
        for r in json.loads((control_path / "angular-report.json").read_text())
        if r["species"] == species
    )
    assert sha(Path(control["cache_path"])) == control["cache_sha256"]
    assert source["input_sha256"] == control["input_sha256"]
    cache = load_views(control["cache_path"], source["identity"])
    image = reproject(
        cache[int(np.argmax(sphere @ validation[5]))], source, validation[5]
    )
    pictures = [references[5], image]
    del cache
    variants = []
    for epsilon in (0.00025, 0.0005):
        began = time.perf_counter()
        patches, remainder = plane_patches(source, epsilon)
        partition_s = time.perf_counter() - began
        began = time.perf_counter()
        products = bake_patches(source, patches)
        build_s = time.perf_counter() - began
        cache_path = OUTPUT / f"{species}-{int(epsilon * 1000000)}um.npz"
        save_product(cache_path, source, products, remainder)
        errors, times, vertex_error = [], [], []
        for index, (direction, reference) in enumerate(
            zip(validation, references, strict=True)
        ):
            began = time.perf_counter()
            candidate = spatial_view(products, remainder, source, direction)
            times.append(time.perf_counter() - began)
            errors.append(compare_images(reference, candidate))
            frame, lower, upper = projection(source["vertices"], direction)
            scale = 64 / (upper - lower)
            maximum = 0.0
            for patch in patches:
                vertices = source["vertices"][source["faces"][patch["faces"]]]
                offsets = vertices @ patch["normal"] - patch["offset_m"]
                shift = offsets[..., None] * (patch["normal"] @ frame[:, :2]) * scale
                maximum = max(maximum, float(np.linalg.norm(shift, axis=-1).max()))
            vertex_error.append(maximum)
            if index == 5:
                pictures.append(candidate)
            if index % 5 == 0:
                print(
                    species,
                    epsilon,
                    "view",
                    index + 1,
                    "of",
                    len(validation),
                    flush=True,
                )
        variants.append(
            {
                "epsilon_m": epsilon,
                "charts": len(patches),
                "source_triangles": len(source["faces"]),
                "untouched_triangles": len(remainder),
                "draw_triangles": 2 * len(patches) + len(remainder),
                "partition_s": partition_s,
                "two_sided_bake_s": build_s,
                "cache_path": str(cache_path),
                "cache_sha256": sha(cache_path),
                "cache_compressed_bytes": cache_path.stat().st_size,
                "cpu_reference_draw_samples_s": times,
                "maximum_projected_vertex_error_px": max(vertex_error),
                "cached_chart_array_bytes": sum(
                    a.nbytes
                    for p in products
                    for v in p["views"]
                    for a in v.values()
                    if isinstance(a, np.ndarray)
                ),
            }
            | error_summary(errors)
        )
        print(
            species,
            epsilon,
            variants[-1]["draw_triangles"],
            variants[-1]["max_absolute_area_error"],
            variants[-1]["max_coverage_l1_relative"],
            flush=True,
        )
    diagnostic(species, pictures)
    return {
        "species": species,
        "identity": source["identity"],
        "axis_path": source["axis_path"],
        "input_sha256": source["input_sha256"],
        "frozen_single_card_control_commit": control["commit"],
        "variants": variants,
        "scope": "Unlit geometry, complete original-face partition and CPU reference costs only; no shading/wind/shadow/native/forest LOD approval.",
    }


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    implementation = [
        *HERE.glob("needle_spatial_*.py"),
        HERE / "needle_composition.py",
        *(
            HERE / n
            for n in (
                "needle_group_layers.py",
                "needle_group_source.py",
                "needle_group_views.py",
                "needle_group_reprojection.py",
                "raster_batch.py",
                "leaf_sheet.py",
                "plant_depth.py",
                "foliage_selection.py",
            )
        ),
    ]
    implementation.extend(
        HERE / n
        for n in (
            "../../../lib/src/outshine_ref/vegetation/shoot_plan.py",
            "../../../lib/src/outshine_ref/vegetation/vector3.py",
        )
    )
    records = []
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    source_modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", *[str(p) for p in implementation]],
            text=True,
        ).strip()
    )
    for species in ("picea-abies", "pinus-sylvestris", "pinus-cembra", "larix-decidua"):
        records.append(
            compare(species)
            | {
                "commit": commit,
                "source_modified": source_modified,
                "implementation_sha256": {
                    str(p.relative_to(HERE)) if ".." in p.parts else p.name: sha(p)
                    for p in implementation
                },
            }
        )
        (OUTPUT / "report.json").write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    main()
