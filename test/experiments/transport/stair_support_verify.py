"""Replay saved piers against actual baseline products, without a support builder."""

import json
import resource

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.transport.supports import SupportVolume
from stair_support_audit import Reference, domain, quantized
from stair_support_experiment import arguments
from stair_support_plan import (
    admit,
    footprint,
    inputs,
    source_surfaces,
    station_choices,
)
from stair_terrain_experiment import frozen
from wien_support_verify import unpack


def replay(rows, road, lower, root, method, note):
    path = root / (method + ".npz")
    if digest(path) != note["mesh_sha256"]:
        raise ValueError("saved stair support products changed")
    bodies, caps, _arrays = unpack(path)
    candidates, short, deferred = station_choices(
        rows, selection=note.get("station_selection", "all")
    )
    if deferred != note.get("deferred_stations", []):
        raise ValueError("support deferred station ledger incomplete")
    if short != note["short_sources"] or len(candidates) != len(note["stations"]):
        raise ValueError("support station and short-source ledger incomplete")
    upper_surfaces, exclusions = source_surfaces(road, {c[0] for c in candidates})
    uppers = {owner: Reference(upper) for owner, upper in upper_surfaces.items()}
    low_ref, shapes, used, checked = Reference(lower), [[], []], 0, []
    allowed = (0,) if method == "fixed" else (0, -2, 2, -4, 4)
    for candidate, row in zip(candidates, note["stations"], strict=True):
        owner, line, nominal = candidate
        trials = [t["shift_m"] for t in row["trials"]]
        if (row["source_id"], row["nominal_m"]) != (
            rows[owner]["source_id"],
            nominal,
        ) or trials != list(allowed[: len(trials)]):
            raise ValueError("support source station or trial ledger changed")
        if "product" not in row:
            if not row["reason"]:
                raise ValueError("support omission reason absent")
            continue
        if (
            row["product"] != used
            or not trials
            or row["shift_m"] != trials[-1]
            or row["reason"]
        ):
            raise ValueError("support product ownership or selected trial changed")
        product = SupportVolume(bodies[used], *caps[used])
        expected, actual = (
            footprint(line, nominal + row["shift_m"]),
            domain(product.lower_contact),
        )
        if (
            expected.symmetric_difference(actual).area > 1e-8
            or expected.hausdorff_distance(actual) > 1e-7
        ):
            raise ValueError("saved pier differs from its source station footprint")
        actual_shapes, physical = admit(
            product, low_ref, uppers[owner], exclusions[owner], shapes
        )
        if physical != row["physical"]:
            raise ValueError("saved pier physical evidence changed")
        for pool, shape in zip(shapes, actual_shapes, strict=True):
            pool.append(shape)
        used += 1
        checked.append(physical)
    if used != len(bodies) or used != note["accepted"]:
        raise ValueError("support emitted bodies and source ledger disagree")
    return bodies, {"accepted": used, "physical": checked}


def verify(args):
    commit = frozen()
    report = json.loads((args.output / "comparison.json").read_text())
    rows, road, lower, context, artifacts = inputs(args.products, args.fixture)
    if (
        report["fixture_sha256"] != digest(args.fixture)
        or report["baseline"] != str(args.products.resolve())
        or report["baseline_report_sha256"] != digest(args.products / "comparison.json")
        or report["baseline_artifacts"] != artifacts
        or report["origin_lat_lon"] != context["origin_lat_lon"]
        or report["admitted_source_ids"]
    ):
        raise ValueError("support baseline/source/frame/non-admission binding changed")
    results, selected = {}, None
    for method in ("fixed", "shifted"):
        selection = json.loads(args.fixture.read_text()).get(
            "support_station_selection", "all"
        )
        if report["methods"][method].get("station_selection", "all") != selection:
            raise ValueError(
                "saved support station selection differs from source recipe"
            )
        bodies, result = replay(
            rows, road, lower, args.output, method, report["methods"][method]
        )
        results[method] = result
        if method == report["selected"]:
            selected = bodies
    if selected is None:
        raise ValueError("unknown selected support method")
    before = dict(np.load(args.products / "after-display.npz"))
    scene = json.loads((args.products / "after-overview-scene.json").read_text())
    expected = append(
        before, [(quantized(body), 6) for body in selected], np.array(scene["colours"])
    )
    shown = dict(np.load(args.output / "after-display.npz"))
    if (
        digest(args.output / "after-display.npz")
        != report["display"]["after_display_sha256"]
        or set(shown) != set(expected)
        or any(not np.array_equal(shown[key], value) for key, value in expected.items())
        or digest(args.output / "before-display.npz")
        != digest(args.products / "after-display.npz")
    ):
        raise ValueError("support display changed existing data or actual new bodies")
    result = {
        "generation_commit": report["commit"],
        "reader_commit": commit,
        "products_report_sha256": digest(args.output / "comparison.json"),
        "methods": results,
        "valid_local_candidate": True,
        "admitted_source_ids": [],
    }
    (args.output / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {key: result[key] for key in ("reader_commit", "valid_local_candidate")}
        ),
        flush=True,
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    verify(arguments())
