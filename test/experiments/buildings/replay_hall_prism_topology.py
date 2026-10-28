"""Exact full-hall products and same-input costs for prism topology preparation."""

import argparse
import hashlib
import json
import os
import platform
import resource
import subprocess
import sys
import time
from dataclasses import asdict
from pathlib import Path
from unittest.mock import patch

import numpy as np
from allocation_measure import allocations
from industrial_hall_plan import metadata
from outshine_ref.buildings.modelling import prism_cut
from outshine_ref.buildings.modelling.prism_source_topology import (
    fan_topology,
    topology,
)
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile
from recipe_graph_data import plain


def arrays(asset):
    meshes = {"body": asset.body, "door": asset.closed_door}
    for group in (
        "fixed",
        "closed_gate_full",
        "closed_gate_flat",
        "full_panels",
        "sheet_panels",
    ):
        meshes.update(
            {f"{group}/{i}": mesh for i, mesh in enumerate(getattr(asset, group))}
        )
    values = {
        f"{name}/{field}": value
        for name, mesh in meshes.items()
        for field, value in mesh.items()
    }
    shell = asset.run.outputs["shell"].value.shell
    values.update(
        {
            f"shell/{name}": getattr(shell, name)
            for name in ("vertex_roots", "source_indices", "source_triangles")
        }
    )
    values.update(field_bounds=asset.field_bounds, gate_bounds=asset.gate_bounds)
    return values


def logical(asset):
    shell = asset.run.outputs["shell"].value.shell
    return {
        "hall": plain(metadata(asset.plan)),
        "gate_definition": asdict(asset.plan["definition"]),
        "entry_id": asset.plan["entry"]["id"],
        "rooflights": plain(asset.run.outputs["rooflights"].value),
        "source_ids": shell.source_ids,
        "omissions": shell.omissions,
        "volume_m3": shell.volume_m3,
        "area_m2": shell.area_m2,
    }


def logical_bytes(asset):
    return json.dumps(
        logical(asset), sort_keys=True, default=lambda value: value.tolist()
    ).encode()


def fingerprint(prepared):
    digest = hashlib.sha256()
    for key, asset in sorted(prepared.assets.items()):
        digest.update(key.encode())
        digest.update(logical_bytes(asset))
        for name, value in sorted(arrays(asset).items()):
            digest.update(name.encode())
            digest.update(value.dtype.str.encode())
            digest.update(str(value.shape).encode())
            digest.update(value.tobytes())
    digest.update(
        json.dumps([dict(r) for r in prepared.rejected], sort_keys=True).encode()
    )
    return digest.hexdigest()


def stage(rows, method):
    work = {
        "cut_prisms": 0,
        "triangle_corners": 0,
        "encoded_property_vertices": 0,
        "topology_root_entries": 0,
        "compact_prisms": 0,
    }
    procedure = topology if method == "compact" else fan_topology

    def counted(polygon, vertices, faces):
        mesh, roots = procedure(polygon, vertices, faces)
        work["cut_prisms"] += 1
        work["triangle_corners"] += int(faces.size)
        work["encoded_property_vertices"] += len(mesh["vertices"])
        work["topology_root_entries"] += len(roots)
        work["compact_prisms"] += int(len(mesh["vertices"]) == 8)
        return mesh, roots

    cpu, wall = time.process_time(), time.perf_counter()
    with patch.object(prism_cut, "topology", counted):
        prepared = prepare_halls(rows)
    return prepared, {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
        "work": work,
    }


def compare(before, after):
    if set(before.assets) != set(after.assets) or before.rejected != after.rejected:
        raise ValueError("Whole original hall ownership or refusals changed")
    fields, payload = 0, 0
    for key, old in before.assets.items():
        new = after.assets[key]
        a, b = arrays(old), arrays(new)
        if logical_bytes(old) != logical_bytes(new):
            raise ValueError(
                "Logical hall programme, source relation or physical measures changed"
            )
        if (
            a.keys() != b.keys()
            or old.run.outputs["shell"].value.shell.source_ids
            != new.run.outputs["shell"].value.shell.source_ids
        ):
            raise ValueError("Hall source/property fields changed")
        for name, value in a.items():
            other = b[name]
            if (
                value.dtype != other.dtype
                or value.shape != other.shape
                or value.tobytes() != other.tobytes()
            ):
                raise ValueError("Complete hall field changed: " + key + ":" + name)
            fields += 1
            payload += value.nbytes
    return {
        "exact_array_fields": fields,
        "logical_array_bytes_including_aliases": payload,
        "source_rows": len(before.rows),
        "building_sources": sum(row["layer"] == "building" for row in before.rows),
        "halls": len(before.assets),
        "rooflight_fields": sum(len(a.field_bounds) for a in before.assets.values()),
    }


def replay(args):
    if subprocess.check_output(["git", "status", "--porcelain"], text=True):
        raise ValueError("Freeze prism topology implementation before evidence")
    args.output.mkdir(parents=True, exist_ok=False)
    cpu = time.process_time()
    rows, receipt, context, _view = source_tile()
    acquisition = time.process_time() - cpu
    methods = ("fans", "compact")
    first = {name: stage(rows, name) for name in methods}
    equality = compare(first["fans"][0], first["compact"][0])
    hashes = {name: fingerprint(value[0]) for name, value in first.items()}
    runs, traces = {name: [] for name in methods}, {}
    for turn in range(args.rounds):
        for name in methods if turn % 2 == 0 else reversed(methods):
            value, cost = stage(rows, name)
            if fingerprint(value) != hashes[name]:
                raise ValueError(
                    "Complete hall products changed during repeat generation"
                )
            runs[name].append(cost)
    if args.trace:
        for name in methods:
            path = args.output / (name + ".bin")
            (value, _), measured = allocations(
                path, lambda name=name: stage(rows, name)
            )
            if fingerprint(value) != hashes[name]:
                raise ValueError("Profiled full hall products changed")
            traces[name] = measured | {
                "scope": "Complete preparation of all original hall assets from already decoded source rows: logical plans, graph/envelope, cuts/body, glazing, full fixed/moving/detail products and retained outputs. Source acquisition, hashing/validation, export and trace processing excluded. Requested bytes are not copied bytes."
            }
    exported = {}
    for key, asset in first["compact"][0].assets.items():
        for name, value in arrays(asset).items():
            exported[key + "/" + name] = value
    np.savez_compressed(args.output / "assets.npz", **exported)
    status = Path("/proc/self/status")
    if status.exists():
        peak = (
            int(
                next(
                    line.split()[1]
                    for line in status.read_text().splitlines()
                    if line.startswith("VmHWM:")
                )
            )
            * 1024
        )
        peak_source = "Linux current-address-space /proc/self/status VmHWM"
    else:
        peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (
            1 if sys.platform == "darwin" else 1024
        )
        peak_source = "getrusage process maximum; may inherit ancestor peak"
    report = {
        "schema": "outshine/hall-prism-topology-comparison/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            name: os.environ.get(name)
            for name in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "source_acquisition_cpu_s": acquisition,
        "source_receipt": receipt,
        "source_context": context,
        "equality": equality,
        "methods": {
            name: {
                "first_cpu_s": first[name][1]["cpu_s"],
                "first_work": first[name][1]["work"],
                "runs": runs[name],
                "allocations": traces.get(name),
                "artifact_sha256": hashes[name],
            }
            for name in methods
        },
        "halls": {
            key: {
                "arrays_prefix": key + "/",
                "logical": logical(asset),
                "body_triangles": len(asset.body["faces"]),
                "volume_m3": asset.run.outputs["shell"].value.shell.volume_m3,
                "area_m2": asset.run.outputs["shell"].value.shell.area_m2,
            }
            for key, asset in first["compact"][0].assets.items()
        },
        "process_peak_rss_bytes": peak,
        "process_peak_source": peak_source,
        "scope": "Repeated complete generator execution after imports/templates, never an asset-cache hit. No source IO/decoding or diagnostic joins in timed stage. Exact topology/material/UV/physical-detail buffers, no device/frame-p99/8GB or whole-world admission. Logical field byte sums can count shared arrays repeatedly; total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(report, indent=2, default=lambda value: value.tolist()) + "\n"
    )
    print(json.dumps(equality))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("Bounded rounds1..16 required")
    replay(args)


if __name__ == "__main__":
    main()
