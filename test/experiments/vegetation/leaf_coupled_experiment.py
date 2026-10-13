"""Refine cached developed primary veins without changing their geometry."""

import hashlib
import json
import os
import resource
from pathlib import Path

import numpy as np
from leaf_closed_experiment import build_leaf


def main():
    source = Path("build/experiments/vegetation/developing-leaves")
    output = source.with_name("coupled-leaves")
    output.mkdir(parents=True, exist_ok=True)
    cases, inputs, products = [], {}, []
    scene = json.loads((source / "leaf-development-detail.json").read_text())
    for index in range(3):
        name = f"developed-{index}"
        material, files, case = build_leaf(source, output, index, name, 4096)
        inputs.update(files)
        recipe = json.loads((output / material).read_text())
        with (
            np.load(output / recipe["vein_plan"]) as primary,
            np.load(output / recipe["minor_network"]) as network,
        ):
            count = len(primary["positions"])
            edges = np.column_stack((primary["parents"][1:], np.arange(1, count)))
            assert np.array_equal(
                network["raw_positions"][:count], primary["positions"]
            )
            assert np.array_equal(network["raw_edges"][: len(edges)], edges)
            assert int(network["base_edges"]) == len(edges)
        case["primary_nodes_preserved"] = count
        cases.append(case)
        product = scene["products"][0] | {"leaf_material": material}
        for key, suffix in (("mesh", "mesh"), ("placements", "poses")):
            path = source / f"{name}-{suffix}.npz"
            inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
            product[key] = os.path.relpath(path, output)
        products.append(product)
        print(index, case, flush=True)
    detail = scene | {"name": "leaf-coupled-detail", "products": products[:1]}
    (output / "leaf-coupled-detail.json").write_text(
        json.dumps(detail, indent=2) + "\n"
    )
    for index, product in enumerate(products):
        with np.load(output / product["placements"]) as saved:
            poses = {key: saved[key] for key in saved.files}
        poses["origins"][:, 0] = (index - 1) * 0.1
        path = output / f"developed-{index}-poses.npz"
        np.savez_compressed(path, **poses)
        product["placements"] = path.name
    wide = json.loads((source / "leaf-material-upper.json").read_text())
    wide.update(name="leaf-coupled-upper", products=products)
    (output / "leaf-material-upper.json").write_text(json.dumps(wide, indent=2) + "\n")
    report = {
        "cases": cases,
        "input_sha256": inputs,
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("leaf_*.py")
        },
    }
    (output / "coupled.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
