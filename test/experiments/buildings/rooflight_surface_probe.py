"""Actual cache-body reuse, directly emitted skins and scoped equal-quality costs."""

import argparse
import gc
import hashlib
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from facade_parts import append
from industrial_hall_details import products
from industrial_hall_shell import structure
from industrial_roof_choice import section_for
from industrial_rooflight_probe import storage_bytes
from industrial_rooflights import components, cuts, plan, templates
from provider_contract import attributes
from recipe_hall_operators import inputs_for, prepare
from rooflight_surfaces import closed, contact, prepare_host, template
from tile_buildings import construct
from tile_roof_envelopes import choose
from verify_industrial_halls import sources


def prepared(row, neighbors):
    started = time.perf_counter()
    run = prepare().evaluate(
        inputs_for(row, neighbors), outputs=("programme", "envelope")
    )
    value = run.outputs["programme"].value.plan
    value = dict(value, entry=dict(value["entry"], angle_deg=90))
    fields = plan(value, section_for(row, choose(row, attributes(row["properties"]))))
    body, shell = structure(
        value, run.outputs["envelope"].value, 1, extra_cuts=cuts(fields, 1)
    )
    body_ms = (time.perf_counter() - started) * 1000
    started = time.perf_counter()
    host = prepare_host(shell, fields)
    return {
        "value": value,
        "fields": fields,
        "body": body,
        "shell": shell,
        "host": host,
        "prepare_body_ms": body_ms,
        "prepare_contacts_ms": (time.perf_counter() - started) * 1000,
        "graph_nodes": run.stats["evaluated_nodes"],
    }


def skin(value, method):
    if method == "cga":
        return components(value["fields"])
    if method == "closed":
        return closed(value["fields"])
    if method == "contact":
        return contact(value["host"])
    raise ValueError("Unknown rooflight surface method")


def emit(value, view, method):
    prior, _ = products(value["value"], 1, 1, view)
    return append(value["body"], [*prior, *skin(value, method)])


def mapped(row, raw, name):
    def builder(_row, _parameters, material):
        mats = raw["materials"].copy()
        mats[mats == 1] = material
        return dict(raw, materials=mats), {
            "form": "Prepared source hall plus directly emitted rooflight product"
        }

    mesh, source = construct(
        name,
        prepared=([row], {"invalid": [], "span_m": 100.0}),
        ordinary_builder=builder,
    )
    if source["generated"] != 1 or source["omissions"]:
        raise ValueError("Incomplete source hall in surface probe")
    return mesh, source


def equal(mesh, record, source):
    if record["source"]["material_definitions"] != source["material_definitions"]:
        raise ValueError("Surface probe changed source palette")
    with np.load(record["mesh"]) as old:
        if set(old.files) != set(mesh):
            raise ValueError("Source render streams differ")
        for key, array in mesh.items():
            if array.dtype != old[key].dtype:
                raise ValueError("Source render precision differs")
            np.testing.assert_array_equal(array, old[key])


def models(references, output, proof):
    row, neighbors = sources()[2]
    value = prepared(row, neighbors)
    output.mkdir(parents=True, exist_ok=True)
    evidence = []
    for suffix in ("Whole", "Near", "Inside", "Opposite"):
        path = references / ("Rooflight2" + suffix + "Glazed-geometry.json")
        record = json.loads(path.read_text())
        camera = record["camera"]
        view = View(
            np.asarray(camera["eye"]),
            np.asarray(camera["target"]),
            width=1920,
            height=1080,
            fov_deg=camera["vertical_fov_deg"],
        )
        expected, source = mapped(row, emit(value, view, "cga"), record["city"])
        equal(expected, record, source)
        evidence.append(
            {"reference": str(path), "all_arrays_dtypes_palette_exact": True}
        )
        methods = ("closed", "contact") if suffix == "Near" else ("contact",)
        for method in methods:
            target = output / method
            target.mkdir(exist_ok=True)
            mesh, source = mapped(row, emit(value, view, method), record["city"])
            npz = target / (record["city"] + "-mesh.npz")
            np.savez_compressed(npz, **mesh)
            changed = (
                record
                | proof
                | {
                    "mesh": str(npz.resolve()),
                    "mesh_sha256": hashlib.sha256(npz.read_bytes()).hexdigest(),
                    "source": source,
                    "triangles": len(mesh["faces"]),
                    "surface_method": method,
                    "prior_reference": str(path),
                    "scope": "Same prepared complete source-bound body/shaft/collision plan; directly emitted closed panes/frame surfaces. Contact variant omits only certified opaque mating faces; no general far/shadow/radiance/device admission.",
                }
            )
            (target / (record["city"] + "-geometry.json")).write_text(
                json.dumps(changed, indent=2) + "\n"
            )
    (output / "reference-equality.json").write_text(
        json.dumps(
            proof
            | {
                "cases": evidence,
                "prepare_body_ms": value["prepare_body_ms"],
                "prepare_contacts_ms": value["prepare_contacts_ms"],
                "graph_nodes": value["graph_nodes"],
            },
            indent=2,
        )
        + "\n"
    )


def costs(output, proof):
    result = []
    for row, neighbors in sources()[:3]:
        value = prepared(row, neighbors)
        measurements = []
        for method in ("cga", "closed", "contact"):
            templates.cache_clear()
            template.cache_clear()
            start = time.perf_counter()
            skin(value, method)
            cold = (time.perf_counter() - start) * 1000
            times = []
            for _ in range(21):
                start = time.perf_counter()
                parts = skin(value, method)
                times.append((time.perf_counter() - start) * 1000)
            del parts
            gc.collect()
            tracemalloc.start()
            parts = skin(value, method)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            measurements.append(
                {
                    "method": method,
                    "cold_ms": cold,
                    "warm_runs_ms": times,
                    "warm_median_ms": float(np.median(times)),
                    "triangles": sum(len(p["faces"]) for p in parts),
                    "components": len(parts),
                    "transformed_position_normal_array_count": 2 * len(parts),
                    "transformed_position_normal_bytes": sum(
                        p[k].nbytes for p in parts for k in ("vertices", "normals")
                    ),
                    "retained_unique_array_bytes": storage_bytes(parts),
                    "traced_peak_bytes": peak,
                    "cga_template_entries": templates.cache_info().currsize,
                    "direct_template_entries": template.cache_info().currsize,
                }
            )
        result.append(
            {
                "source_id": row["id"],
                "fields": len(value["fields"]),
                "prepare_body_ms": value["prepare_body_ms"],
                "prepare_contacts_ms": value["prepare_contacts_ms"],
                "graph_nodes": value["graph_nodes"],
                "measurements": measurements,
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "costs.json").write_text(
        json.dumps(
            proof
            | {
                "cases": result,
                "process_peak_rss_kib": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
                "scope": "Normal-priority shared-host emission only; cache-body/source/CSG/contact preparation reported separately, not removed. Known final transforms/storage/traced peak; total native allocation/copy/work unknown. No complete pipeline/native/device speed claim.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--references", type=Path)
    args = parser.parse_args()
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze surface implementation")
    if args.references:
        models(args.references, args.output, proof)
    costs(args.output, proof)
