"""Actual v2 hall replay, fresh processes and equal-product graph/domain costs."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from recipe_graph_data import canonical, plain
from recipe_hall_operators import (
    choice_operator,
    envelope_operator,
    inputs_for,
    programme_operator,
)
from recipe_rooflight_operators import (
    glazed_operator,
    logical_operator,
    panel_operator,
    prepare,
)
from recipe_rooflight_products import assemble
from rooflight_surface_probe import equal, mapped
from verify_industrial_halls import sources
from verify_recipe_halls import mesh_equal


def direct(inputs, full):
    choice = choice_operator({}, {"source": inputs["source"]})["choice"]
    programme = programme_operator({}, inputs | {"choice": choice})["programme"]
    glazing = logical_operator(
        {"max_fields": 64}, inputs | {"choice": choice, "programme": programme}
    )["rooflights"]
    result = {"programme": programme, "rooflights": glazing}
    if not full:
        return result
    envelope = envelope_operator(
        {"wall_material": 1}, {"source": inputs["source"], "choice": choice}
    )["envelope"]
    shell = glazed_operator({"wall_material": 1}, result | {"envelope": envelope})[
        "shell"
    ]
    panels = panel_operator({}, {"rooflights": glazing})["panels"]
    return result | {"envelope": envelope, "shell": shell, "panels": panels}


def summary(run):
    return {
        "definition": run.definition_fingerprint,
        "instance": run.instance_fingerprint,
        "stats": plain(run.stats),
        "products": {
            k: {
                "fingerprint": p.fingerprint,
                "type": p.type_id,
                "bytes": p.nbytes,
                "metadata": plain(p.metadata),
            }
            for k, p in run.outputs.items()
        },
        "fields": plain(run.outputs["rooflights"].value),
        "room_id": run.outputs["programme"].value.plan["room_id"],
    }


def worker(output, index):
    row, neighbors = sources()[index]
    run = prepare().evaluate(inputs_for(row, neighbors))
    output.mkdir(parents=True, exist_ok=True)
    arrays = dict(run.outputs["shell"].value.body)
    for i, part in enumerate(run.outputs["panels"].value):
        arrays |= {f"panel-{i}-{k}": v for k, v in part.items()}
    np.savez_compressed(output / f"{index}.npz", **arrays)
    (output / f"{index}.json").write_text(json.dumps(summary(run), indent=2) + "\n")


def fresh(output):
    proofs = []
    for i in range(3):
        for repeat in range(2):
            subprocess.run(
                [
                    sys.executable,
                    __file__,
                    "--output",
                    str(output / str(repeat)),
                    "--worker",
                    str(i),
                ],
                check=True,
                capture_output=True,
                env=os.environ.copy(),
            )
        a, b = (output / str(j) for j in range(2))
        if (a / f"{i}.json").read_bytes() != (b / f"{i}.json").read_bytes():
            raise ValueError("Fresh v2 process changed actual logical products")
        with np.load(a / f"{i}.npz") as first, np.load(b / f"{i}.npz") as last:
            mesh_equal(dict(first), dict(last))
            proofs.append(
                {
                    "case": i,
                    "arrays": len(first.files),
                    "logical_and_dtypes_exact": True,
                }
            )
    return proofs


def costs():
    rows = []
    for i, (row, neighbors) in enumerate(sources()[:3]):
        inputs, graph = inputs_for(row, neighbors), prepare()
        for full in (False, True):
            requested = None if full else ("programme", "rooflights")
            expected = direct(inputs, full)
            actual = graph.evaluate(inputs, outputs=requested)
            if canonical(expected["rooflights"].value) != canonical(
                actual.outputs["rooflights"].value
            ):
                raise ValueError("Unequal logical cost comparison")
            if full:
                mesh_equal(
                    expected["shell"].value.body, actual.outputs["shell"].value.body
                )
                for a, b in zip(
                    expected["panels"].value,
                    actual.outputs["panels"].value,
                    strict=True,
                ):
                    mesh_equal(a, b)
            for method in ("domain", "graph"):

                def measure(
                    inputs=inputs,
                    full=full,
                    method=method,
                    graph=graph,
                    requested=requested,
                ):
                    return (
                        direct(inputs, full)
                        if method == "domain"
                        else graph.evaluate(inputs, outputs=requested).outputs
                    )

                times = []
                for _ in range(21):
                    began = time.perf_counter()
                    result = measure()
                    times.append((time.perf_counter() - began) * 1000)
                tracemalloc.start()
                result = measure()
                _, peak = tracemalloc.get_traced_memory()
                tracemalloc.stop()
                rows.append(
                    {
                        "case": i,
                        "full": full,
                        "method": method,
                        "runs_ms": times,
                        "median_ms": float(np.median(times)),
                        "traced_peak_bytes": peak,
                        "accounted_bytes": sum(p.nbytes for p in result.values()),
                        "evaluated_nodes": actual.stats["evaluated_nodes"]
                        if method == "graph"
                        else None,
                    }
                )
    return rows


def models(folder, output, proof):
    results = []
    for i, (row, neighbors) in enumerate(sources()[:3]):
        run = prepare().evaluate(inputs_for(row, neighbors))
        for path in sorted(folder.glob(f"Rooflight{i}*Glazed-geometry.json")):
            record = json.loads(path.read_text())
            camera = record["camera"]
            view = View(
                np.asarray(camera["eye"]),
                np.asarray(camera["target"]),
                width=1920,
                height=1080,
                fov_deg=camera["vertical_fov_deg"],
            )
            raw, _, _ = assemble(run, view, opened=1, staff_angle=90)
            mesh, source = mapped(row, raw, record["city"])
            target = output / (record["city"] + "-mesh.npz")
            np.savez_compressed(target, **mesh)
            value = (
                record
                | proof
                | {
                    "mesh": str(target.resolve()),
                    "mesh_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                    "source": source,
                    "triangles": len(mesh["faces"]),
                    "v2_recipe": summary(run),
                }
            )
            (output / path.name).write_text(json.dumps(value, indent=2) + "\n")
            results.append(
                {
                    "case": i,
                    "view": record["city"],
                    "triangles": len(mesh["faces"]),
                    "fields": len(run.outputs["rooflights"].value),
                }
            )
    return results


def replay(path):
    record = json.loads(path.read_text())
    c = record["camera"]
    row, neighbors = sources()[2]
    run = prepare().evaluate(inputs_for(row, neighbors))
    view = View(
        np.asarray(c["eye"]),
        np.asarray(c["target"]),
        width=1920,
        height=1080,
        fov_deg=c["vertical_fov_deg"],
    )
    raw, _, _ = assemble(run, view, opened=1, staff_angle=90)
    mesh, source = mapped(row, raw, record["city"])
    equal(mesh, record, source)
    return {"reference": str(path), "all_arrays_dtypes_palette_exact": True}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--references", type=Path)
    parser.add_argument("--replay", type=Path, action="append", default=[])
    parser.add_argument("--worker", type=int)
    args = parser.parse_args()
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze v2 recipe before evidence")
    args.output.mkdir(parents=True, exist_ok=True)
    if args.worker is not None:
        worker(args.output, args.worker)
    else:
        record = proof | {
            "fresh": fresh(args.output / "fresh"),
            "costs": costs(),
            "replay": [replay(p) for p in args.replay],
            "scope": "Normal-priority shared-host equal-product comparison; domain vs graph logical/full, actual accounted outputs and traced peak. Source preparation and unmeasured native/allocation/copy traffic are not removed; no GPU/native/cache ABI or frame admission.",
        }
        if args.references:
            record["models"] = models(args.references, args.output, proof)
        (args.output / "evidence.json").write_text(json.dumps(record, indent=2) + "\n")
