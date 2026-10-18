"""Pack an actual composed Place view for Blender rendering on another host."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import bpy
from place_camera import PLACES
from render_place import compose, digest


def export(name, sources, products, output):
    scene, report = compose(name, sources, products, output)
    began = time.perf_counter()
    bpy.ops.file.pack_all()
    images = []
    for image in bpy.data.images:
        if image.source == "FILE":
            if image.packed_file is None:
                raise ValueError(f"image is not packed: {image.name}")
            images.append(
                {
                    "name": image.name,
                    "size": list(image.size),
                    "packed_bytes": image.packed_file.size,
                    "packed_sha256": hashlib.sha256(image.packed_file.data).hexdigest(),
                }
            )
    target = (output / f"{name}.blend").resolve()
    bpy.ops.wm.save_as_mainfile(
        filepath=str(target), compress=True, check_existing=False
    )
    report.update(
        artifact_kind="packed-static-reference-scene",
        scene_path=str(target),
        scene_sha256=digest(target),
        scene_bytes=target.stat().st_size,
        blender_version=bpy.app.version_string,
        samples=scene.eevee.taa_render_samples,
        packed_images=images,
        export_s=time.perf_counter() - began,
        render_completed=False,
        portability="Native Blender scene and packed images; no source receipts, GIS modules or Python generator needed to render. Static camera/shadow selection, not a complete runtime world asset.",
    )
    (output / f"{name}-scene.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "stage": "export",
                "path": str(target),
                "scene_bytes": report["scene_bytes"],
                "images": len(images),
                "export_s": report["export_s"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--place", required=True, choices=PLACES)
    parser.add_argument("--sources", required=True, type=Path)
    parser.add_argument("--products", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    export(args.place, args.sources, args.products, args.output)
