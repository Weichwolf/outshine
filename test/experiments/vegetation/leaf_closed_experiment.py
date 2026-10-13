"""Compare complete closed-vein growth and export replayable material products."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from blade_surface import Blade
from leaf_closed_checks import closed_checks, same_network
from leaf_closed_material import add_minor_material
from leaf_closed_venation import grow_closed
from leaf_network import node_network, topology
from leaf_venation import leaf_sources


def build_leaf(source_folder, output, index):
    name = f"leaf-{index}"
    path = source_folder / f"{name}-material.json"
    material = json.loads(path.read_text())
    blade = Blade(**material["blade"])
    plan_path = source_folder / material["vein_plan"]
    assert (
        hashlib.sha256(plan_path.read_bytes()).hexdigest()
        == material["vein_plan_sha256"]
    )
    with np.load(plan_path) as saved:
        positions = saved["positions"]
        edges = np.column_stack((saved["parents"][1:], np.arange(1, len(positions))))
    sources = leaf_sources(blade, 166 + index, 64)
    times, results = {m: [] for m in ("dense", "sectors")}, {}
    for repeat in range(3):
        order = ("dense", "sectors") if repeat % 2 == 0 else ("sectors", "dense")
        for method in order:
            start = time.perf_counter()
            results[method] = grow_closed(blade, positions, edges, sources, method)
            times[method].append(time.perf_counter() - start)
        same_network(results["dense"], results["sectors"])
    network = results["sectors"]
    start = time.perf_counter()
    nodes, segments, graph = node_network(network.positions, network.edges)
    noding_seconds = time.perf_counter() - start
    assert graph["components"] == 1 and graph["cycles"] > 0
    indices = {tuple(p): i for i, p in enumerate(nodes)}
    hubs = np.array(
        [indices[tuple(network.positions[h])] if h >= 0 else -1 for h in network.hubs]
    )
    np.savez_compressed(
        output / f"{name}-network.npz",
        positions=nodes,
        edges=segments,
        sources=sources,
        hubs=hubs,
        raw_hubs=network.hubs,
        base_edges=np.array(network.base_edges),
        raw_positions=network.positions,
        raw_edges=network.edges,
        first_reach=network.first_reach,
        completed=network.completed,
    )
    start = time.perf_counter()
    filename, inputs, field = add_minor_material(
        output, source_folder, name, blade, material, network
    )
    material_seconds = time.perf_counter() - start
    for p in (path, plan_path):
        inputs[str(p)] = hashlib.sha256(p.read_bytes()).hexdigest()
    return (
        filename,
        inputs,
        {
            "seconds": times,
            "same_growth": True,
            "raw": topology(network.positions, network.edges),
            "noded": graph,
            "nodes": len(nodes),
            "edges": len(segments),
            "remaining_sources": network.remaining_sources,
            "stop_reason": network.stop_reason,
            "rejected_segments": network.rejected_segments,
            "noding_seconds": noding_seconds,
            "material_seconds": material_seconds,
            **field,
        },
    )


def main():
    source = Path("build/experiments/vegetation/leaf-materials")
    output = Path("build/experiments/vegetation/closed-leaves")
    output.mkdir(parents=True, exist_ok=True)
    materials, inputs, cases = [], {}, []
    for index in range(3):
        material, files, case = build_leaf(source, output, index)
        materials.append(material)
        inputs.update(files)
        cases.append(case)
        print(index, case, flush=True)
    for view in ("upper", "lower", "coarse", "detail"):
        base_view = "upper" if view == "detail" else view
        path = source / f"leaf-material-{base_view}.json"
        inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        recipe = json.loads(path.read_text())
        for index, product in enumerate(recipe["products"]):
            for key in ("mesh", "placements"):
                p = source / product[key]
                inputs[str(p)] = hashlib.sha256(p.read_bytes()).hexdigest()
                product[key] = "../leaf-materials/" + product[key]
            product["leaf_material"] = materials[index]
        recipe["name"] = f"closed-leaf-{view}"
        if view == "detail":
            recipe.update(
                camera=[-0.1, 0.024, 0.245],
                target=[-0.1, 0.065, 0.112],
                products=recipe["products"][:1],
            )
        (output / f"closed-leaf-{view}.json").write_text(
            json.dumps(recipe, indent=2) + "\n"
        )
    names = [p.name for p in Path(__file__).parent.glob("leaf_closed*.py")]
    names += [
        "leaf_network.py",
        "leaf_domain.py",
        "leaf_relative_neighbors.py",
        "leaf_vein_field.py",
        "leaf_venation.py",
        "blade_surface.py",
    ]
    report = {
        "cases": cases,
        "checks": closed_checks(),
        "input_sha256": inputs,
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in names
        },
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
    }
    (output / "closed-leaves.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
