"""Fresh-process recipe replay, actual render-input equivalence and scoped graph overhead."""

import argparse
import gc
import json
import os
import resource
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from industrial_hall_plan import metadata
from recipe_graph_data import canonical, plain
from recipe_hall_operators import (
    choice_operator,
    envelope_operator,
    inputs_for,
    prepare,
    programme_operator,
    shell_operator,
)
from tile_buildings import construct
from verify_industrial_halls import sources
from verify_recipe_halls import assembly, mesh_equal


def direct(inputs, *, full=True):
    choice = choice_operator({}, {"source": inputs["source"]})["choice"]
    programme = programme_operator({}, inputs | {"choice": choice})["programme"]
    if not full:
        return {"programme": programme}
    envelope = envelope_operator(
        {"wall_material": 1}, {"source": inputs["source"], "choice": choice}
    )["envelope"]
    shell = shell_operator(
        {"wall_material": 1}, {"envelope": envelope, "programme": programme}
    )["shell"]
    return {"programme": programme, "envelope": envelope, "shell": shell}


def worker(output, index):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before recipe evidence")
    row, neighbors = sources()[index]
    started = time.perf_counter()
    graph = prepare()
    prepare_ms = (time.perf_counter() - started) * 1000
    inputs = inputs_for(row, neighbors)
    scale = 1 if sys.platform == "darwin" else 1024
    baseline = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * scale
    started = time.perf_counter()
    run = graph.evaluate(inputs)
    evaluation_ms = (time.perf_counter() - started) * 1000
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * scale
    mesh = run.outputs["shell"].value.body
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / f"{index}-shell.npz", **mesh)
    record = proof | {
        "definition": run.definition_fingerprint,
        "instance": run.instance_fingerprint,
        "products": {
            name: {
                "fingerprint": p.fingerprint,
                "type": p.type_id,
                "nbytes": p.nbytes,
                "metadata": plain(p.metadata),
            }
            for name, p in run.outputs.items()
        },
        "plan": json.loads(canonical(metadata(run.outputs["programme"].value.plan))),
        "stats": plain(run.stats),
        "prepare_ms": prepare_ms,
        "evaluation_ms": evaluation_ms,
        "before_evaluation_peak_rss_bytes": baseline,
        "process_peak_rss_bytes": peak,
    }
    (output / f"{index}-recipe.json").write_text(json.dumps(record, indent=2) + "\n")


def fresh(output):
    result = []
    for index in range(3):
        records = []
        for repeat in range(2):
            folder = output / f"restart-{repeat}"
            command = [
                sys.executable,
                __file__,
                "--output",
                str(folder),
                "--worker",
                str(index),
            ]
            subprocess.run(
                command,
                check=True,
                env=os.environ.copy(),
                capture_output=True,
            )
            records.append(json.loads((folder / f"{index}-recipe.json").read_text()))
        for name in ("definition", "instance", "products", "plan", "stats"):
            if records[0][name] != records[1][name]:
                raise ValueError("Fresh recipe restart changed " + name)
        with (
            np.load(output / "restart-0" / f"{index}-shell.npz") as a,
            np.load(output / "restart-1" / f"{index}-shell.npz") as b,
        ):
            mesh_equal(dict(a), dict(b))
        result.append(
            {
                "case": index,
                "source_id": sources()[index][0]["id"],
                "exact": True,
                "stats": records[0]["stats"],
            }
        )
    return result


def render_equality(folder):
    proofs = []
    cached = {}
    for index in range(3):
        row, neighbors = sources()[index]
        cached[index] = prepare().evaluate(inputs_for(row, neighbors))
    for path in sorted(folder.glob("Hall*-geometry.json")):
        record = json.loads(path.read_text())
        name = record["city"]
        if name.endswith("Before"):
            continue
        index = int(name[4])
        row, _ = sources()[index]
        camera = record["camera"]
        view = View(
            np.asarray(camera["eye"]),
            np.asarray(camera["target"]),
            width=camera["profile"][0],
            height=camera["profile"][1],
            fov_deg=camera["vertical_fov_deg"],
        )
        opened = 0 if name.endswith("Closed") else 0.5 if name.endswith("Half") else 1
        raw, _collision, _value = assembly(cached[index], view, opened)

        def builder(_row, _parameters, material, raw=raw):
            mapped = raw["materials"].copy()
            mapped[mapped == 1] = material
            return dict(raw, materials=mapped), {"form": "Recipe equivalence control"}

        mesh, source = construct(
            name,
            prepared=([row], {"invalid": [], "span_m": 100.0}),
            ordinary_builder=builder,
        )
        if source["generated"] != 1 or source["omissions"]:
            raise ValueError("Incomplete recipe render control")
        with np.load(record["mesh"]) as old:
            mesh_equal(dict(old), mesh)
        proofs.append(
            {
                "model": str(path),
                "mesh_sha256": record["mesh_sha256"],
                "all_streams_dtypes_exact": True,
                "camera": record["camera"],
                "wall_binding": "semantic slot1 rebound to the same source palette by existing tile constructor",
            }
        )
    if len(proofs) != 8:
        raise ValueError(
            "Expected eight actual hall/state/interior/opposite source render inputs"
        )
    return proofs


def costs():
    result = []
    for index, (row, neighbors) in enumerate(sources()[:3]):
        inputs = inputs_for(row, neighbors)
        graph = prepare()
        for full in (False, True):
            expected = direct(inputs, full=full)
            for mode in ("direct", "graph"):

                def run(mode=mode, full=full, inputs=inputs, graph=graph):
                    return (
                        direct(inputs, full=full)
                        if mode == "direct"
                        else graph.evaluate(
                            inputs, outputs=None if full else ("programme",)
                        ).outputs
                    )

                actual = run()
                if canonical(metadata(actual["programme"].value.plan)) != canonical(
                    metadata(expected["programme"].value.plan)
                ):
                    raise ValueError("Cost controls have different logical plans")
                if full:
                    mesh_equal(actual["shell"].value.body, expected["shell"].value.body)
                for _ in range(2):
                    run()
                times = []
                for _ in range(21):
                    start = time.perf_counter()
                    actual = run()
                    times.append((time.perf_counter() - start) * 1000)
                del actual
                gc.collect()
                tracemalloc.start()
                actual = run()
                current, peak = tracemalloc.get_traced_memory()
                blocks = sum(
                    s.count for s in tracemalloc.take_snapshot().statistics("filename")
                )
                tracemalloc.stop()
                arrays = actual["shell"].value.body if full else {}
                result.append(
                    {
                        "case": index,
                        "mode": mode,
                        "full": full,
                        "p50_ms": float(np.median(times)),
                        "runs_ms": times,
                        "domain_operator_calls": 4 if full else 2,
                        "definition_bytes": graph.definition_bytes
                        if mode == "graph"
                        else 0,
                        "accounted_returned_product_bytes": sum(
                            v.nbytes for v in actual.values()
                        ),
                        "body_arrays": len(arrays),
                        "body_array_bytes": sum(v.nbytes for v in arrays.values()),
                        "body_xyz_write_bytes": arrays["vertices"].nbytes
                        if full
                        else 0,
                        "traced_live_bytes": current,
                        "traced_peak_bytes": peak,
                        "traced_live_blocks": blocks,
                    }
                )
    return {
        "cases": result,
        "conditions": "Normal priority, shared-host load, 2 warmups/21 runs; setup/loading excluded; tracemalloc separate",
        "scope": "Same actual source plan and shell/attributes. Direct invokes the same four trusted domain callbacks but omits graph typing/fingerprints; measured graph overhead is not an equal-feature evaluator contest or native speed factor. Only accounted descriptor/array bytes, known final writes and live traced allocations; total copies, Python/native heap and solver scratch remain unenumerated.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--worker", type=int, choices=range(3))
    parser.add_argument("--render-inputs", type=Path)
    args = parser.parse_args()
    if args.worker is not None:
        worker(args.output, args.worker)
    else:
        proof = revision()
        if proof["experiment_tree_dirty"]:
            raise ValueError("Freeze source before recipe evidence")
        args.output.mkdir(parents=True, exist_ok=True)
        report = proof | {"restarts": fresh(args.output), "costs": costs()}
        if args.render_inputs:
            report["render_equivalence"] = render_equality(args.render_inputs)
        (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
