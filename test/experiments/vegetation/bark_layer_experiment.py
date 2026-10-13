"""Compare complete bark fields on the same connected graph; emit current layered products."""

import hashlib
import json
import resource
import shutil
import subprocess
import time
from pathlib import Path

import numpy as np
from bark_layer_checks import check_layers
from bark_layers import bark_layers
from graph_wood_axes import wood_axes
from graph_wood_surface import joined_wood
from spatial_examples import spatial_graph


def compare(axes, mesh):
    samples = {method: [] for method in ("dense", "indexed")}
    outputs = {}
    for _ in range(3):
        for method, times in samples.items():
            began = time.perf_counter()
            outputs[method] = bark_layers(axes, mesh, method)
            times.append(time.perf_counter() - began)
    checks = check_layers(axes, mesh, outputs["dense"][0], outputs["indexed"][0])
    winner = min(samples, key=lambda method: np.median(samples[method]))
    return outputs[winner][0], {
        "samples_s": samples,
        "work": {m: v[1] for m, v in outputs.items()},
        "checks": checks,
        "selected": winner,
    }


def emit(output, mesh, layers, count):
    source = output.parent / "graph-wood"
    # Existing geometry/organ products stay byte-identical, with a new complete material product.
    for path in source.iterdir():
        if path.suffix in (".npz", ".json") and path.name not in (
            "wood-functions.json",
            "verification.json",
        ):
            shutil.copy2(path, output / path.name)
    np.savez(
        output / "layered-near-wood-mesh.npz",
        **{k: mesh[k] for k in ("vertices", "faces", "normals")},
        **layers,
    )
    material = json.loads((source / "bark-material.json").read_text())
    material |= {"version": "layered-bark-preview-1", "layers": count}
    (output / "layered-bark-material.json").write_text(
        json.dumps(material, indent=2) + "\n"
    )
    for name in (
        "joined-spatial-graph",
        "joined-spatial-detail",
        "joined-spatial-junction",
    ):
        path = output / f"{name}.json"
        recipe = json.loads(path.read_text())
        recipe["source_commit"] = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip()
        recipe["source_modified"] = True
        recipe["products"][0]["mesh"] = "layered-near-wood-mesh.npz"
        recipe["products"][0]["bark_material"] = "layered-bark-material.json"
        path.write_text(json.dumps(recipe, indent=2) + "\n")


def main():
    output = Path("build/experiments/vegetation/bark-layers")
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    for seed in (17, 23, 47):
        if seed == 17:
            with np.load(
                output.parent / "graph-wood/joined-near-wood-mesh.npz"
            ) as data:
                mesh = {k: data[k] for k in data.files}
            axes, _ = wood_axes(spatial_graph(seed))
        else:
            product = joined_wood(spatial_graph(seed))
            mesh, axes = product[0], product[2]
        layers, report = compare(axes, mesh)
        reports.append({"seed": seed, **report})
        if seed == 17:
            emit(output, mesh, layers, len(axes))
    names = (
        "bark_layers.py",
        "bark_layer_checks.py",
        "bark_layer_experiment.py",
        "graph_wood_axes.py",
        "spine_closest.py",
    )
    report = {
        "seeds": reports,
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in names
        },
    }
    (output / "bark-layers.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
