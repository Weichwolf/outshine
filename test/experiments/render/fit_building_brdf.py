"""Offline source-measure response fitting, with original parent assets preserved."""

import argparse
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

import numpy as np
from building_brdf_basis import angular_samples, response_basis
from building_brdf_fit import emitted_error, fit_weights, interval_pair
from building_brdf_highs import highs_pair
from process_memory import peak_rss_bytes


def read_material(path):
    keys = ("m__roughness_weights", "m__albedo", "m__roughness_palette", "m__supported")
    with np.load(path, allow_pickle=False) as archive:
        return {key: archive[key] for key in keys}


def read_fit(path, source, shape, budget):
    with np.load(path, allow_pickle=False) as archive:
        weights = archive["weights"]
        digest = archive["source_node_sha256"].tobytes()
    if digest != hashlib.sha256(source.read_bytes()).digest():
        raise ValueError("fitted material belongs to a different original node")
    if (
        weights.shape != shape
        or weights.dtype != np.float32
        or not np.isfinite(weights).all()
        or np.any(weights < 0)
        or np.any(
            abs(weights.astype(float).sum(axis=-1) - 1) > 8 * np.finfo(np.float32).eps
        )
        or np.any(np.count_nonzero(weights, axis=-1) > budget)
    ):
        raise ValueError(
            "fitted material must preserve its declared positive palette budget"
        )
    return weights


def fit_materials(weights, albedo, train, validation, budget, solver):
    result, reports = weights.copy(), []
    for group, roles in enumerate(weights):
        for role, original in enumerate(roles):
            start = time.process_time()
            fitted, work = fit_weights(
                original,
                albedo[group, role],
                train,
                max_lobes=budget,
                pair_solver=solver,
            )
            mass = float(original.astype(float).sum())
            residual = emitted_error(original, fitted, albedo[group, role], validation)
            if (
                np.any(fitted < 0)
                or abs(float(fitted.astype(float).sum()) - 1)
                > 8 * np.finfo(np.float32).eps
                or np.count_nonzero(fitted) > budget
            ):
                raise ValueError(
                    "emitted Float32 weights must remain a bounded positive material measure"
                )
            result[group, role] = fitted
            reports.append(
                {
                    "group": group,
                    "role": role,
                    "original_active_lobes": int(np.count_nonzero(original)),
                    "fitted_active_lobes": int(np.count_nonzero(fitted)),
                    "original_weights": original.tolist(),
                    "fitted_weights": fitted.tolist(),
                    "work": work,
                    "training_maximum_emitted_relative_channel_error": float(
                        emitted_error(
                            original, fitted, albedo[group, role], train
                        ).max()
                    ),
                    "validation_maximum_relative_channel_error": float(residual.max()),
                    "validation_mean_relative_channel_error": float(residual.mean()),
                    "mass_difference": float(fitted.astype(float).sum() - mass),
                    "cpu_ms": (time.process_time() - start) * 1000,
                }
            )
    return result, reports


def run_case(root, output, args, tables, bases):
    case = json.loads((root / "report.json").read_text())
    output.mkdir()
    results = []
    for name in case["node_files"].values():
        path = root / name
        data = read_material(path)
        if not data["m__supported"].all():
            raise ValueError(
                "complete source material measures required before fitting"
            )
        palette = data["m__roughness_palette"]
        key = palette.tobytes()
        if key not in bases:
            bases[key] = [
                response_basis(palette, tables, angular_samples(validation=v))
                for v in (False, True)
            ]
        fitted, report = fit_materials(
            data["m__roughness_weights"],
            data["m__albedo"],
            *bases[key],
            args.max_lobes,
            interval_pair if args.method == "interval" else highs_pair,
        )
        digest = hashlib.sha256(path.read_bytes()).digest()
        destination = output / name
        np.savez(
            destination,
            weights=fitted,
            source_node_sha256=np.frombuffer(digest, np.uint8),
        )
        results.append(
            {
                "node_file": name,
                "source_node_sha256": digest.hex(),
                "raw_source_container_bytes_hashed": path.stat().st_size,
                "material_array_bytes_decoded": sum(v.nbytes for v in data.values()),
                "fitted_array_bytes": fitted.nbytes,
                "original_array_bytes": data["m__roughness_weights"].nbytes,
                "sidecar_container_bytes": destination.stat().st_size,
                "materials": report,
            }
        )
    return {"cell": case["cell"], "nodes": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-lobes", type=int, choices=(1, 2), default=2)
    parser.add_argument("--method", choices=("interval", "highs"), default="interval")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu, load = time.perf_counter(), time.process_time(), os.getloadavg()
    tables_path = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    with np.load(tables_path, allow_pickle=False) as archive:
        tables = dict(archive)
    bases = {}
    source = json.loads((args.input / "report.json").read_text())
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_producer": source["producer"],
        "source_archive_sha256": source["source_archive_sha256"],
        "method": args.method,
        "max_lobes": args.max_lobes,
        "energy_table_sha256": hashlib.sha256(tables_path.read_bytes()).hexdigest(),
        "response_program_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in ("material_filter.py", "building_mass_lighting.py")
        },
        "cells": [
            run_case(
                args.input / f"cell-{c}", args.output / f"cell-{c}", args, tables, bases
            )
            for c in args.cells
        ],
        "response_palettes_prepared": len(bases),
        "known_basis_array_bytes": sum(
            v.nbytes for pair in bases.values() for b in pair for v in b.values()
        ),
        "basis_lobe_evaluations_once": sum(
            b["specular"].size for pair in bases.values() for b in pair
        ),
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
            "load_before": load,
        },
        "scope": "Offline fits of cached source-informed parent material measures only; no BSC3/source reads, geometry-array decode, mesh construction or camera/light-specific variants. Binding hashes still read full raw node containers, explicitly paid. Same positive roughness palette, albedo and diffuse model. Enumerate all palette pairs on a fixed directional grid; objective is worst relative total direct-response channel error (minimum albedo channel), held-out shifted angles separate. Finite samples and emitted Float32 error only, no physical/global error certificate or native admission. Original node assets retained; separate weight sidecars are experimental, not a cache/GPU format. Dense nine-weight layout is unchanged, so no storage/IO-byte saving claimed. Solvers never run in rendering/loading.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {"method": args.method, "max_lobes": args.max_lobes, "cost": result["cost"]}
        )
    )


if __name__ == "__main__":
    main()
