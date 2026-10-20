"""All delivered poses: filtered normal measures against area-integrated opaque GGX."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_plane_cloud import image_roi, select_full_poses
from full_view_reference import load, views
from PIL import Image, ImageDraw
from triangle_image_reference import prepare_source
from visible_normal_bake import cook
from visible_normal_compare import compare, display, geometry_control, references
from visible_normal_material import tables
from visible_normal_query import load_product


def image_panel(path, images):
    pixels = [images[k] for k in ("fine", "mixture", "mean_toksvig")]
    pixels = [display(x) for x in pixels]
    pixels.append(np.clip(8 * abs(pixels[1] - pixels[0]), 0, 1))
    canvas = Image.new("RGB", (1600, 250), "white")
    draw = ImageDraw.Draw(canvas)
    labels = (
        "fine GGX (visible area)",
        "normal measure",
        "mean + Toksvig",
        "measure error x8",
    )
    for i, (label, data) in enumerate(zip(labels, pixels, strict=True)):
        image = Image.fromarray(np.uint8(np.clip(data, 0, 1) * 255 + 0.5))
        scale = min(390 / image.width, 200 / image.height)
        image = image.resize(
            (int(image.width * scale), int(image.height * scale)),
            Image.Resampling.NEAREST,
        )
        canvas.paste(image, (400 * i + 5, 35))
        draw.text((400 * i + 5, 5), label, fill="black")
    canvas.save(path)


def inputs(path):
    source, document, context, receipt = load(path)
    poses = views(document, context)
    observers = np.array([v[0] for v in poses.values()])
    domain = np.stack((observers.min(0), observers.max(0)))
    selected = select_full_poses(source, poses)
    orient = next(iter(selected.values()))
    for i in range(8):
        reverse = int(f"{i:03b}"[::-1], 2)
        fraction = np.array([(i + 0.5) / 8, (reverse + 0.5) / 8, 0.5])
        poses[f"domain-interior-{i}"] = (
            domain[0] + fraction * np.ptp(domain, axis=0),
            *orient[1:],
        )
    profile = {"width": int(orient[3]), "height": int(orient[4]), "focal_px": orient[2]}
    provenance = {
        "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "parent": document["parent"],
        "view": document["view_key"],
    }
    return (
        source,
        document,
        context,
        receipt,
        (selected | poses),
        selected,
        observers,
        domain,
        profile,
        provenance,
    )


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
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
    up, east, north = (
        np.array(context["scene_frame"][k]) for k in ("y_ecef", "x_ecef", "z_ecef")
    )
    start = time.perf_counter()
    header = cook(
        source,
        domain,
        up,
        profile,
        document["sources"],
        output / "product",
        provenance,
        opaque_control=True,
    )
    bake_s = time.perf_counter() - start
    product = load_product(output / "product", observers, provenance)
    energy, prepared = tables(), prepare_source(source)
    suns = {"day": up + 0.5 * east + 0.3 * north, "side": up - 0.7 * east - 0.3 * north}
    report = {
        "product": header,
        "native_input": receipt,
        "bake_s": bake_s,
        "derived_cpu_bytes": product["derived_cpu_bytes"],
        "matched_homogeneous_geometry_control": geometry_control(
            source, document["sources"], output / "geometry-control.npz"
        ),
        "shared_ggx_table_array_bytes": sum(
            np.asarray(v).nbytes for v in energy.values()
        ),
        "poses": {},
    }
    name, view = next(iter(selected.items()))
    request = {
        "view": [np.asarray(v).tolist() for v in view],
        "roi": image_roi(source, view).tolist(),
        "light": suns["day"].tolist(),
        "roughness": 0.12,
        "provenance": provenance,
    }
    (output / "cache-request.json").write_text(json.dumps(request, indent=2) + "\n")
    unobserved = {r["ordinal"] for r in header["unsampled_sources"]}
    for name, view in poses.items():
        box = image_roi(source, view)
        if box is None:
            report["poses"][name] = {"offscreen": True}
            continue
        programs = references(source, view, box, prepared if name in selected else None)
        pose = {"roi": box.tolist(), "reference": programs[3], "cases": {}}
        pose["observed_without_training_hit"] = sorted(
            unobserved.intersection(programs[3]["observed_sources"])
        )
        for light_name, light in suns.items():
            for roughness in (0.12, 0.45):
                key = f"{light_name}-r{str(roughness).replace('.', 'p')}"
                result, images = compare(
                    source,
                    product,
                    view,
                    box,
                    programs,
                    light,
                    roughness,
                    energy,
                    check_sampling=name in selected,
                )
                pose["cases"][key] = result
                if name in selected or name == "domain-interior-0":
                    np.savez_compressed(output / f"{name}-{key}.npz", **images)
                    image_panel(output / f"{name}-{key}.png", images)
        report["poses"][name] = pose
        print(
            json.dumps(
                {
                    "pose": name,
                    "reference_s": pose["reference"]["visibility_and_quadrature_s"],
                    "coverage_l1_px": [
                        c["coverage_l1_px"] for c in pose["cases"].values()
                    ],
                }
            ),
            flush=True,
        )
    report.update(
        query_work=product["work"],
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "terminal": "complete",
                "poses": len(report["poses"]),
                "product_bytes": header["stored_array_bytes"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
