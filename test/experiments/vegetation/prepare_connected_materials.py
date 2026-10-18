"""Prepare bounded original-material fields from finished carrier recipes and inputs."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from connected_material_fields import build_material
from PIL import Image


def original_material(inputs, study):
    source = json.loads((study / "report.json").read_text())["source"]
    material = source["material_inputs"]
    folder = inputs / "spatial"
    for name, digest in material["original_files_sha256"].items():
        if hashlib.sha256((folder / name).read_bytes()).hexdigest() != digest:
            raise ValueError("original material file identity differs")
    adapter = Path(__file__).with_name("fern_material.py")
    if hashlib.sha256(adapter.read_bytes()).hexdigest() != material["adapter_sha256"]:
        raise ValueError("original material adapter changed")
    metadata = json.loads((folder / "fern-material.json").read_text())
    if metadata != material["original_recipe"]:
        raise ValueError("original material metadata differs")
    colour = np.asarray(Image.open(folder / metadata["colour"]))[::-1] / 255
    height = np.asarray(Image.open(folder / metadata["height"]))[::-1] / 65535
    identity = hashlib.sha256(json.dumps(material, sort_keys=True).encode()).hexdigest()
    return colour, height, identity, material


def prepare(inputs, study, output, *, size=32, order=4):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    colour, height, identity, material = original_material(inputs, study)
    output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    products = []
    for variant in range(8):
        path = study / "connected32" / f"prototype-{variant}.json"
        carrier = json.loads(path.read_text())
        before = time.perf_counter()
        arrays, description = build_material(
            carrier["recipe"],
            colour,
            height,
            size=size,
            order=order,
            material_identity=identity,
        )
        elapsed = time.perf_counter() - before
        description |= {
            "carrier_identity": carrier["identity"],
            "build_s": elapsed,
            "source_faces": carrier["source_triangles"],
            "capture_texels": (size * order) ** 2,
            "array_bytes": sum(a.nbytes for a in arrays.values()),
        }
        np.savez_compressed(output / f"material-{variant}.npz", **arrays)
        (output / f"material-{variant}.json").write_text(
            json.dumps(description, indent=2) + "\n"
        )
        products.append(description)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "material_identity": identity,
        "material_inputs": material,
        "products": products,
        "wall_s": time.perf_counter() - start,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Eight bounded original coarse prototypes; no hosts/poses or whole crown generated. Original maps are authored procedural inputs, not a calibrated botanical BSDF. Same connected32 geometry for all material resolutions.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("inputs", "study", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    p.add_argument("--size", type=int, choices=(16, 32, 64), default=32)
    p.add_argument("--order", type=int, choices=(2, 4, 8), default=4)
    a = p.parse_args()
    prepare(a.inputs, a.study, a.output, size=a.size, order=a.order)
