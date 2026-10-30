"""Existing whole Buildings generation after bound, genuinely elevated products."""

import argparse
import json
import os
import time
from pathlib import Path
from types import SimpleNamespace

from camera_height_inputs import digest
from process_memory import peak_rss_bytes
from rail_occupancy_guard import planning
from rail_occupancy_inputs import catalog, load
from rail_occupancy_reference import RailReference, whole_raised
from stair_consumer_buildings import body_query
from stair_consumer_experiment import save
from stair_consumer_inputs import candidate_inputs
from stair_consumer_verify import check_bodies, meshes
from stair_occupancy_candidate_consumers import generate as generate_buildings
from stair_occupancy_handoff import OccupancyQuery
from stair_support_audit import quantized
from stair_terrain_experiment import frozen


def bound_products(report):
    root = Path(report["elevated_supports"])
    if (
        digest(root / "report.json") != report["elevated_report_sha256"]
        or digest(root / "verification.json") != report["elevated_receipt_sha256"]
    ):
        raise ValueError("Same whole elevated input and reader receipt required")
    return load(root)


def generate(args):
    frozen()
    start, cpu, loadavg = time.perf_counter(), time.process_time(), os.getloadavg()
    rail = load(args.supports)
    data = candidate_inputs(args.baseline, rail.state)
    prepared = time.perf_counter()
    args.output.mkdir(parents=True, exist_ok=False)
    inputs = SimpleNamespace(
        output=args.output,
        baseline=args.baseline,
        resident=Path(json.loads((args.supports / "report.json").read_text())["deck"]),
        prior_module=[],
    )
    # The old ordered module chain is retained; elevated bodies are separate inputs.
    deck_report = json.loads((inputs.resident / "report.json").read_text())
    inputs.resident = Path(deck_report["resident"])
    inputs.prior_module = [Path(p["path"]) for p in deck_report["prior_modules"]]
    report = generate_buildings(
        inputs,
        rail.state,
        data,
        rail.originals,
        catalog=catalog(rail),
        query_factory=lambda prepared: planning(rail, prepared=prepared),
    )
    report.update(
        elevated_supports=str(args.supports.resolve()),
        elevated_report_sha256=digest(args.supports / "report.json"),
        elevated_receipt_sha256=digest(args.supports / "verification.json"),
        separate_elevated_source_ids=[r["source_id"] for r in rail.rows],
        cost={
            "input_prepare_wall_s": prepared - start,
            "total_wall_s": time.perf_counter() - start,
            "total_cpu_s": time.process_time() - cpu,
            "process_peak_rss_bytes": peak_rss_bytes(),
            "loadavg_before": loadavg,
        },
        scope="Actual existing Buildings recipes consume unchanged114 old sources plus separate qualified raised deck/pier/headroom volumes. No Ground conversion, new Source/World admission or native/device proof.",
    )
    save(args.output, "comparison.json", report)
    return report


def verify(args):
    commit = frozen()
    report = json.loads((args.products / "comparison.json").read_text())
    rail = bound_products(report)
    baseline = Path(report["baseline"])
    if (
        digest(baseline / "comparison.json") != report["baseline_report_sha256"]
        or report["source_ids"] != rail.state[4]["source_ids"]
        or report["separate_elevated_source_ids"] != [r["source_id"] for r in rail.rows]
        or report["frame"] != rail.state[0].patches[0].frame.identifier
    ):
        raise ValueError("Whole old/elevated source/frame bindings differ")
    expected = {
        "buildings-before.npz",
        "buildings-after.npz",
        "building-ledger.json",
        "context.json",
        "building-reservations.json",
    }
    if set(report["artifacts"]) != expected:
        raise ValueError("Whole original building products required")
    for name, sha in report["artifacts"].items():
        if digest(args.products / name) != sha:
            raise ValueError("Actual saved building bytes changed: " + name)
    after = meshes(args.products / "buildings-after.npz")
    result = check_bodies(
        candidate_inputs(baseline, rail.state),
        json.loads((args.products / "building-ledger.json").read_text()),
        meshes(args.products / "buildings-before.npz"),
        after,
        json.loads((args.products / "building-reservations.json").read_text()),
        RailReference(baseline, rail),
        planning_query=planning(rail, baseline=baseline),
    )
    rounded = OccupancyQuery(whole_raised(rail, cast=True)[0])
    for index, body in enumerate(after):
        _, footprint = body_query(
            f"actual-rounded-building:{index}", quantized(body), 0
        )
        conflict, _work = rounded.conflict(footprint, method="scan")
        if not conflict.is_empty:
            raise ValueError(f"Float32 building conflicts with raised bodies: {index}")
    result.update(
        reader_commit=commit,
        producer_commit=report["commit"],
        report_sha256=digest(args.products / "comparison.json"),
        float32_bodies_against_actual_raised_volumes=True,
        scope="Fresh whole saved-body/source/root/full-volume reader; all old partitions plus independently scanned actual raised caps/headroom. Actual rounded buildings also tested against rounded new volumes; no full old-world Float32/native/Source/World/device admission.",
    )
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--supports", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit system-temp output required")
    if not args.products and (args.supports is None or args.baseline is None):
        parser.error("Generation requires --supports and --baseline")
    result = verify(args) if args.products else generate(args)
    print(json.dumps(result, indent=2))
