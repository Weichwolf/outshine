"""Cache two complete floral phases and compare one-organ demand before/after meshing."""

import argparse
import json
import os
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
from raceme_mesh import phase_mesh
from raceme_phase import prepare
from understory_leaf_maps import write_maps


def crop(mesh, ranges, identity):
    row = next(r for r in ranges if tuple(r["id"]) == identity)
    begin, end = row["vertex_range"]
    faces = mesh["faces"]
    mask = ((faces >= begin) & (faces < end)).all(axis=1)
    return {
        "vertices": mesh["vertices"][begin:end].copy(),
        "faces": faces[mask] - begin,
        "normals": mesh["normals"].reshape(-1, 3, 3)[mask].reshape(-1, 3),
        "uv": mesh["uv"].reshape(-1, 3, 2)[mask].reshape(-1, 2),
        "surface": mesh["surface"][mask],
    }


def compare_demand(recipe):
    identity, phase = (3, 8), 0.95

    def full_then_crop():
        mesh, ranges, _ = phase_mesh(recipe, phase)
        return crop(mesh, ranges, identity)

    def demand_first():
        return phase_mesh(recipe, phase, selected=[identity])[0]

    a, b = full_then_crop(), demand_first()
    assert set(a) == set(b)
    for key in a:
        assert (
            a[key].dtype == b[key].dtype
            and a[key].shape == b[key].shape
            and a[key].tobytes() == b[key].tobytes()
        ), key
    methods = {"full_then_crop": full_then_crop, "demand_first": demand_first}
    samples = {name: [] for name in methods}
    for iteration in range(6):
        for name in list(methods)[:: 1 if iteration % 2 else -1]:
            start = time.perf_counter()
            methods[name]()
            samples[name].append(time.perf_counter() - start)
    traced = {}
    for name, method in methods.items():
        tracemalloc.start()
        product = method()
        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        traced[name] = {
            "retained_traced_bytes": current,
            "peak_traced_bytes": peak,
            "output_payload_bytes": sum(x.nbytes for x in product.values()),
        }
    return {
        "requested_organ": identity,
        "exact_arrays": True,
        "timings_s": samples,
        "tracemalloc_separate_untimed_calls": traced,
        "output_payload_bytes": sum(x.nbytes for x in a.values()),
        "scope": "Same requested complete organ arrays. Traced Python/registered NumPy memory is not all allocator calls, copy traffic or process RSS.",
    }


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    recipe = prepare(121)
    (output / "raceme-plan.json").write_text(json.dumps(recipe, indent=2) + "\n")
    write_maps(output)
    records = []
    for label, phase in (("early", 0.95), ("late", 1.3)):
        folder = output / label
        folder.mkdir()
        start = time.perf_counter()
        mesh, ranges, state = phase_mesh(recipe, phase)
        elapsed = time.perf_counter() - start
        np.savez_compressed(folder / "convallaria-near.npz", **mesh)
        np.savez_compressed(folder / "phase-arrays.npz", **state)
        for name in (
            "parallel-leaf-colour.png",
            "parallel-leaf-height.png",
            "parallel-leaf-material.json",
        ):
            os.link(output / name, folder / name)
        record = {
            "phase": phase,
            "open": int(np.count_nonzero(state["active"] & state["open"])),
            "buds": int(np.count_nonzero(state["active"] & ~state["open"])),
            "vertices": len(mesh["vertices"]),
            "triangles": len(mesh["faces"]),
            "payload_bytes": sum(v.nbytes for v in mesh.values()),
            "mesh_s": elapsed,
            "organ_ranges": ranges,
            "scope": recipe["scope"],
        }
        (folder / "maturation-state.json").write_text(
            json.dumps(record, indent=2) + "\n"
        )
        records.append(record)
    summary = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "phases": records,
        "one_organ": compare_demand(recipe),
    }
    (output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
