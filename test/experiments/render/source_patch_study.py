"""Whole original adjacency evidence before choosing a surface-patch approximation."""

import argparse
import hashlib
import json
import math
import platform
import subprocess
import time
from collections import Counter
from fractions import Fraction as F
from pathlib import Path

import numpy as np
from building_sample_originals import read_original
from experiment_output import create_output
from source_patch_candidates import input_parts, plane, prepare

ROOT = Path(__file__).resolve().parents[3]


def adjacency(source, candidate):
    fields, normals, owners = input_parts(source)
    points, point_ids = candidate["integer_points"], candidate["point_ids"]
    grid = F(2) ** candidate["position_grid_exponent"]
    edge_owner = {}
    keys = []
    counts = Counter()
    maximum_squared = F(0)
    for index, face in enumerate(source["indices"]):
        vertices = list(map(int, face))
        key = plane(tuple(points[v] for v in vertices))
        if key is None:
            keys.append(None)
            continue
        n = normals[face].view("u4")
        uv = fields[face, 0]
        codes = (
            np.where(uv < 0, uv, np.floor(uv / np.float32(256))).astype("f4").view("u4")
        )
        keys.append((key, tuple(map(int, n[0])), tuple(map(int, codes))))
        for a, b in (
            (vertices[0], vertices[1]),
            (vertices[1], vertices[2]),
            (vertices[2], vertices[0]),
        ):
            edge = (
                owners[index],
                min(point_ids[a], point_ids[b]),
                max(point_ids[a], point_ids[b]),
            )
            if edge not in edge_owner:
                edge_owner[edge] = index
                continue
            previous = edge_owner[edge]
            other = keys[previous]
            if other is None:
                continue
            counts["shared_same_part_edge_pairs"] += 1
            if other[1] != keys[-1][1]:
                counts["different_original_normal"] += 1
                continue
            if len({*other[2], *keys[-1][2]}) != 1:
                counts["different_or_nonuniform_native_flat_code"] += 1
                continue
            if other[0] == key:
                counts["same_plane_normal_code"] += 1
                continue
            counts["same_normal_code_but_different_exact_plane"] += 1
            coefficients = other[0][0]
            denominator = sum(x * x for x in coefficients[:3])
            for v in vertices:
                residual = (
                    sum(x * y for x, y in zip(coefficients[:3], points[v], strict=True))
                    + coefficients[3]
                )
                squared = F(residual**2, denominator) * grid**2
                maximum_squared = max(maximum_squared, squared)
                if squared:
                    counts["nonzero_plane_residual_vertices"] += 1
    return {
        "pairs": dict(counts),
        "maximum_corresponding_plane_distance_m": math.sqrt(float(maximum_squared)),
        "scope": "Exact encoded-plane equality and exact squared distances; displayed square root is approximate, no tolerance-based admission.",
    }


def run(args):
    files = [
        Path(__file__).with_name(n)
        for n in (
            "source_patch_candidates.py",
            "source_patch_study.py",
            "building_sample_originals.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze the measured sources first")
    create_output(args.output)
    source, provenance = read_original(
        ROOT / "test/outshine/data/render/building-depth-samples/original-geometry",
        attributes=True,
    )
    start = time.process_time()
    candidate = prepare(source)
    cpu = time.process_time() - start
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "platform": platform.platform(),
        "provenance": provenance,
        "work": candidate["work"],
        "adjacency": adjacency(source, candidate),
        "classification_single_cpu_s": cpu,
        "scope": "Whole unmodified original source. Structural development/eligibility evidence, not compact asset, repeated cost comparison, GPU/World/p99/UMA qualification.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {"work": result["work"], "adjacency": result["adjacency"], "cpu_s": cpu}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
