"""Replay saved layered bodies without invoking a profile solver or mesh generator."""

import json
import resource
import subprocess
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_verify import read_ribbon, scene_proof
from layered_bridge_audit import audit
from layered_bridge_experiment import arguments
from layered_bridge_inputs import base_inputs, retained_proof
from layered_bridge_plan import lower_bound
from layered_bridge_problem import prepare
from layered_bridge_replay import pier_proof, source_proof
from layered_bridge_topology import topology
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from wien_support_verify import unpack


def separation(lower, upper):
    result = {}
    for mode in ("double", "float32"):
        a, b = lower.upper, upper.lower
        if mode == "float32":
            a = Mesh(a.vertices.astype(np.float32).astype(float), a.triangles)
            b = Mesh(b.vertices.astype(np.float32).astype(float), b.triangles)
        note = vertical_clearance(a, b, seconds=4, max_faces=8192)
        if not note["valid"] or note["minimum_clearance_m"] < 4.5 - 1e-7:
            raise ValueError("saved actual lower traffic envelope is not clear")
        result[mode] = note
    return result


def saved_profile(args, fixture, name, lower, arrays, report):
    row = report["candidates"][name]
    for filename, key in (
        (name + ".npz", "body_sha256"),
        (name + "-piers.npz", "piers_sha256"),
    ):
        if digest(args.output / filename) != row[key]:
            raise ValueError("saved layered body/pier export binding changed")
    p = prepare(fixture, name, lower_bound(fixture, lower) if lower else ())
    for key in ("stations", "lower", "upper", "rate"):
        if not np.array_equal(arrays[name + "_" + key], p[key]):
            raise ValueError("saved measured input differs from whole-source problem")
    ribbon, table = read_ribbon(args.output / (name + ".npz"))
    h = arrays[name + "_heights"]
    if not np.allclose(
        h, np.interp(p["stations"], table[:, 0], table[:, 1]), rtol=0, atol=1e-8
    ):
        raise ValueError("saved height table differs from its measured solution")
    physical = audit(ribbon, table, p, fixture)
    note = {"geometry": {"table": table.tolist()}, "physical": physical}
    return (p, h, ribbon, note), physical


def verify(args):
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            "lib",
            str(Path(__file__).resolve().parent),
        ],
        text=True,
    ).strip():
        raise ValueError("saved layered replay requires frozen clean research sources")
    report = json.loads((args.output / "comparison.json").read_text())
    before, ground, borders = base_inputs(args)
    fixture = json.loads(args.fixture.read_text())
    if report["dirty"] or digest(args.fixture) != report["fixture_sha256"]:
        raise ValueError("dirty or unbound saved layered source")
    for file, expected in (
        ("after-body.npz", report["presentation"]["after_body_sha256"]),
        ("source-usage.json", report["source_usage_sha256"]),
        ("profile-input.npz", report["profile_input_sha256"]),
    ):
        if digest(args.output / file) != expected:
            raise ValueError("saved whole-scene/profile/source binding changed")
    field = Mesh(np.array(fixture["field_vertices"]), np.array(fixture["field_faces"]))
    chains, travel = topology(fixture, prepare(fixture, "motorway")["axis"])
    if travel != report["attempted_source_flow"]:
        raise ValueError("saved original source travel binding differs")
    with np.load(args.output / "profile-input.npz") as data:
        arrays = dict(data)
    admitted, checks, bodies = {}, {}, []
    names = report["admitted_profiles"]
    if names != [n for n in ("path", "motorway") if n in names]:
        raise ValueError("saved staged admission order/uniqueness differs")
    for name in ("path", "motorway"):
        if name not in names:
            row = report["candidates"][name]
            if (
                row["valid"]
                or not row.get("reason")
                or (args.output / (name + ".npz")).exists()
            ):
                raise ValueError(
                    "rejected optional profile lacks a valid local fallback"
                )
            continue
        lower = admitted.get("path") if name == "motorway" else None
        value, physical = saved_profile(args, fixture, name, lower, arrays, report)
        _patch, physical["retained"] = retained_proof(
            args, value, ground, borders, field
        )
        piers, contacts, _d = unpack(args.output / (name + "-piers.npz"))
        physical["supports"] = pier_proof(
            fixture, value, chains, piers, contacts, ground, lower
        )
        if lower:
            physical["lower_traffic"] = separation(lower[2], value[2])
        admitted[name] = value
        checks[name] = physical
        bodies.extend([(value[2].body, 3), *[(p, 10) for p in piers]])
    keys = {
        n + "_" + k
        for n in names
        for k in ("stations", "lower", "upper", "rate", "heights")
    }
    if set(arrays) != keys:
        raise ValueError("saved profile inputs include missing/unbound stages")
    palette = np.array(
        json.loads((args.baseline / "after-scene.json").read_text())["colours"]
    )
    scene_proof(args.output / "after-body.npz", before, bodies, palette)
    checks["source_usage"] = source_proof(args, fixture, admitted, report["candidates"])
    if checks["source_usage"]["used_sources"] != report["used_sources"]:
        raise ValueError("saved source count differs from actual complete ledger")
    checks.update(
        valid=True,
        generation_commit=report["commit"],
        reader_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
    )
    (args.output / "verification.json").write_text(json.dumps(checks, indent=2) + "\n")
    print(json.dumps(checks), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3, 4 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    verify(arguments())
