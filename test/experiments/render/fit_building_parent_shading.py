"""Compile existing fixed one/two-lobe controls for a reusable regional case."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_brdf_basis import angular_samples, response_basis
from building_brdf_fit import interval_pair
from compare_mass_appearance import read
from fit_building_brdf import fit_materials, read_material
from process_memory import peak_rss_bytes


def compile_states(source, output, tables_path):
    output.mkdir(parents=True, exist_ok=False)
    cpu, wall = time.process_time(), time.perf_counter()
    case = json.loads((source / "report.json").read_text())
    tables = read(tables_path)
    bases = {}
    records = {1: [], 2: []}
    for budget in records:
        (output / str(budget)).mkdir()
    for name in case["node_files"].values():
        path = source / name
        material = read_material(path)
        if not material["m__supported"].all():
            raise ValueError(
                "complete source material measures required before fitting"
            )
        palette = material["m__roughness_palette"]
        key = palette.tobytes()
        if key not in bases:
            bases[key] = [
                response_basis(palette, tables, angular_samples(validation=v))
                for v in (False, True)
            ]
        digest = hashlib.sha256(path.read_bytes()).digest()
        for budget, rows in records.items():
            fitted, work = fit_materials(
                material["m__roughness_weights"],
                material["m__albedo"],
                *bases[key],
                budget,
                interval_pair,
            )
            destination = output / str(budget) / name
            np.savez(
                destination,
                weights=fitted,
                source_node_sha256=np.frombuffer(digest, np.uint8),
            )
            rows.append(
                {
                    "node_file": name,
                    "source_node_sha256": digest.hex(),
                    "materials": work,
                    "raw_source_container_bytes_hashed": path.stat().st_size,
                    "material_arrays_decoded_once_bytes": sum(
                        v.nbytes for v in material.values()
                    ),
                    "dense_fitted_weight_bytes": fitted.nbytes,
                    "sidecar_container_bytes": destination.stat().st_size,
                }
            )
    programs = {
        name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
        for name in ("material_filter.py", "building_mass_lighting.py")
    }
    for budget, rows in records.items():
        receipt = {
            "max_lobes": budget,
            "energy_table_sha256": hashlib.sha256(tables_path.read_bytes()).hexdigest(),
            "response_program_sha256": programs,
            "nodes": rows,
        }
        (output / str(budget) / "report.json").write_text(
            json.dumps(receipt, indent=2) + "\n"
        )
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": case.get("cells", [case.get("cell")]),
        "budgets": records,
        "shared_response_palettes": len(bases),
        "shared_basis_ggx_evaluations": sum(
            b["specular"].size for pair in bases.values() for b in pair
        ),
        "known_basis_array_bytes": sum(
            a.nbytes for pair in bases.values() for b in pair for a in b.values()
        ),
        "raw_source_container_bytes_hashed_once": sum(
            r["raw_source_container_bytes_hashed"] for r in records[1]
        ),
        "material_arrays_decoded_once_bytes": sum(
            r["material_arrays_decoded_once_bytes"] for r in records[1]
        ),
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Same fixed one/two-lobe finite response fits as #1321, shared basis/material reads; raw node binding hashes still read whole source containers. No geometry/source-form decode, new quality parameter, native cache/binding format or universal response bound. Dense weight layout unchanged.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    tables = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    result = compile_states(args.input, args.output, tables)
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
