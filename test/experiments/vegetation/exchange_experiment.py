"""Measure preparation, storage, current direct light and geometric RGB exchange."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from exchange_basis import prepare_stand, source_nodes
from exchange_io import write_exchange
from exchange_light import basis_optics, direct_nodes
from exchange_operator import build_links, gather_apply, mix_sides, prepare_csr
from grass_light_experiment import light_states


def run(inputs, output, samples, grid):
    if output.exists():
        raise ValueError("fresh finite exchange experiment output required")
    output.mkdir(parents=True)
    timings = {}
    began = time.perf_counter()
    scene, basis = prepare_stand(inputs, grid)
    timings["source_scene_basis_s"] = time.perf_counter() - began
    began = time.perf_counter()
    links = build_links(
        scene,
        basis["points"],
        basis["normals"],
        samples,
        lambda i, p: source_nodes(i, p, basis),
    )
    timings["geometric_links_s"] = time.perf_counter() - began
    began = time.perf_counter()
    header = write_exchange(output / "product", basis, links, samples, 997, 2e-6)
    timings["store_s"] = time.perf_counter() - began
    began = time.perf_counter()
    matrix = prepare_csr(links)
    timings["prepare_csr_s"] = time.perf_counter() - began
    control = json.loads(
        (Path(__file__).parent / "grass_light_control.json").read_text()
    )
    states = light_states(control)
    states["back-bright"] = json.loads(json.dumps(states["back"]))
    for light in states["back-bright"]["lights"]:
        light["irradiance_rgb"] = (2 * np.asarray(light["irradiance_rgb"])).tolist()
    states["back-material"] = json.loads(json.dumps(states["back"]))
    rt = np.asarray(states["back-material"]["leaf_rt"])
    rt[:3] *= 0.5
    rt[3:] *= 1.5
    states["back-material"]["leaf_rt"] = rt.tolist()
    began = time.perf_counter()
    direct = direct_nodes(scene, basis, states)
    timings["current_direct_light_s"] = time.perf_counter() - began
    frames = {}
    apply_times = {}
    for name, state in states.items():
        optical = basis_optics(basis, state)
        began = time.perf_counter()
        incoming = matrix @ direct[name]
        result = mix_sides(incoming, optical)
        apply_times[name] = time.perf_counter() - began
        frames[name + "_direct"] = direct[name]
        frames[name + "_first"] = result
    np.testing.assert_array_equal(frames["back-bright_first"], 2 * frames["back_first"])
    began = time.perf_counter()
    gather = gather_apply(links, direct["back"])
    timings["gather_apply_s"] = time.perf_counter() - began
    np.testing.assert_allclose(gather, matrix @ direct["back"], rtol=0, atol=1e-14)
    np.savez_compressed(output / "frame-diagnostics.npz", **frames)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "product_identity": header["identity"],
        "stand_identity": basis["stand_identity"],
        "geometry_sha256": basis["geometry_sha256"],
        "samples": samples,
        "ground_grid": grid,
        "nodes": len(basis["points"]),
        "csr_nonzeros": matrix.nnz,
        "timings": timings,
        "current_material_exchange_s": apply_times,
        "states": states,
        "nearest_work": scene.work,
        "direct_shadow_work": scene.shadow_work,
        "links_bytes": links.nbytes,
        "basis_bytes": basis["points"].nbytes + basis["normals"].nbytes,
        "csr_bytes": matrix.data.nbytes + matrix.indices.nbytes + matrix.indptr.nbytes,
        "max_row_mass": float(np.max(np.asarray(matrix.sum(1)))),
        "product_file_bytes": (output / "product/geometry.npz").stat().st_size,
        "index_setup": scene.index.setup,
        "scope": "Experimental face-centre/ground-grid approximation of the first exchange only. Geometry-only operator excludes current light/material/camera; fixed static finite geometry, no wind/world/LOD/device admission.",
    }
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "product_identity",
                    "nodes",
                    "samples",
                    "timings",
                    "current_material_exchange_s",
                )
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--samples", type=int, default=4)
    p.add_argument("--ground-grid", type=int, default=64)
    a = p.parse_args()
    run(a.inputs, a.output, a.samples, a.ground_grid)
