"""Actual source stepped plans: envelope measures/sections, real access and rejected alternatives."""

import argparse
import json
import resource
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely
from evidence import revision
from layered_envelope import layered_envelope
from opening_details import slots
from outshine_ref.buildings.modelling import PrismVolume, union_prisms
from showcase_tiles import view_for
from stepped_controls import negative
from stepped_envelope_geometry import geometry as complete_geometry
from stepped_openings import apply as opening_patches
from stepped_source import exterior, layers, near, plan_for
from structural_shell_geometry import mesh_section
from tile_buildings import SITES, read_tile
from tile_facades import contexts
from tile_stepped import candidates
from verify_boolean_surfaces import measure
from verify_vertical import connected_spaces


def envelope_metrics(sources, envelope):
    actual = measure(SimpleNamespace(mesh=envelope.mesh, roots=envelope.vertex_roots))
    volume = sum(s.polygon.area * (s.high_m - s.low_m) for s in sources)
    area = (
        sources[0].polygon.area
        + sources[-1].polygon.area
        + sum(s.polygon.length * (s.high_m - s.low_m) for s in sources)
    )
    area += sum(
        a.polygon.difference(b.polygon).area for a, b in zip(sources, sources[1:])
    )
    np.testing.assert_allclose(actual, [volume, area], atol=1e-7, rtol=1e-9)
    maximum = 0.0
    for source in sources:
        section = mesh_section(
            envelope.mesh, envelope.vertex_roots, (source.low_m + source.high_m) / 2
        )
        maximum = max(maximum, source.polygon.symmetric_difference(section).area)
    if maximum > 1e-7:
        raise ValueError("Stepped exterior differs from its declared layer section")
    return dict(volume_m3=volume, area_m2=area, maximum_section_error_m2=maximum)


def geometry(plan, envelope):
    result = envelope_metrics(layers(plan), envelope)
    for record in plan["terrace_access"]:
        current = plan["storey_plans"][record["storey"]]
        below = mesh_section(
            envelope.mesh, envelope.vertex_roots, current["floor_z_m"] - 0.01
        )
        above = mesh_section(
            envelope.mesh, envelope.vertex_roots, current["floor_z_m"] + 0.01
        )
        if (
            current["terrace"].difference(below).area > 1e-7
            or current["terrace"].intersection(above).area > 1e-7
        ):
            raise ValueError(
                "Far terrace is not flush with its actual near indoor floor"
            )
    return result


def benchmark(declared, identifier, repeats):
    records = []
    for name, method in (
        ("experimental-direct", layered_envelope),
        ("prism-union", union_prisms),
    ):
        try:
            sample = method(declared)
            metrics = envelope_metrics(declared, sample)
        except ValueError as error:
            records.append(
                dict(id=identifier, method=name, status="rejected", reason=str(error))
            )
            continue
        times = []
        for index in range(repeats + 2):
            began = time.perf_counter()
            method(declared)
            if index >= 2:
                times.append((time.perf_counter() - began) * 1000)
        records.append(
            dict(
                id=identifier,
                method=name,
                status="pass",
                samples=repeats,
                p50_ms=float(np.percentile(times, 50)),
                p95_ms=float(np.percentile(times, 95)),
                triangles=len(sample.mesh["faces"]),
                **metrics,
            )
        )
    return records


def run(output, sites, repeats):
    output.mkdir(parents=True, exist_ok=True)
    cases = []
    rejected = []
    receipts = []
    synthetic = [
        PrismVolume(
            str(i),
            shapely.box(i * 0.8, i * 0.8, 18 - i * 0.8, 14 - i * 0.8),
            3 * i,
            3 * (i + 1),
        )
        for i in range(3)
    ]
    timings = (
        benchmark(synthetic, "analytical-18x14-3-storeys", repeats) if repeats else []
    )
    controls = []
    for site in sites:
        rows, receipt = read_tile(site)
        contexts(rows, receipt)
        receipts.append(receipt["receipt"])
        selected = candidates(rows, view_for(rows))
        for row in rows:
            if row["id"] not in selected:
                continue
            started = time.perf_counter()
            try:
                plan = plan_for(row)
                outer, envelope, guard, comparison = exterior(
                    plan, 20, inspect_layers=True
                )
                near_mesh, access, terraces, light = near(plan, 20)
            except (ValueError, AssertionError, shapely.errors.GEOSException) as error:
                rejected.append(dict(id=row["id"], reason=str(error)))
                print(row["id"], "rejected", str(error), flush=True)
                continue
            metrics = geometry(plan, envelope)
            complete = complete_geometry(plan, guard)
            spaces = connected_spaces(plan)
            patched, opening_records = opening_patches(plan, outer, 32768)
            if [r["id"] for r in opening_records] != [r["id"] for r in slots(plan)]:
                raise ValueError("Far patches changed actual near opening identity")
            if len(patched["faces"]) - len(outer["faces"]) != len(opening_records) * 2:
                raise ValueError(
                    "Far opening patch exceeded its two-triangle representation"
                )
            if not controls:
                controls = negative(plan)
            np.testing.assert_array_equal(
                [outer["vertices"][:, 2].min(), outer["vertices"][:, 2].max()],
                [0.0, row["properties"]["render_height"]],
            )
            np.testing.assert_array_equal(
                [near_mesh["vertices"][:, 2].min(), near_mesh["vertices"][:, 2].max()],
                [0.0, row["properties"]["render_height"]],
            )
            if len({r["room_id"] for r in light}) != len(light):
                raise ValueError("Stepped night fixtures lost room identity")
            record = dict(
                id=row["id"],
                storeys=plan["storeys"],
                rooms=sum(len(r) for r in plan["storey_room_ids"]),
                spaces=spaces,
                form_recipe=plan["form_recipe"],
                terrace_access=plan["terrace_access"],
                access=access,
                terrace_mesh_poses=terraces,
                source_properties=row["properties"],
                near_triangles=len(near_mesh["faces"]),
                outer_triangles=len(outer["faces"]),
                far_openings=len(opening_records),
                patched_triangles=len(patched["faces"]),
                exterior_method=comparison,
                complete_exterior=complete,
                seconds=time.perf_counter() - started,
                **metrics,
            )
            cases.append(record)
            print(
                row["id"],
                "pass",
                plan["storeys"],
                "storeys",
                len(terraces),
                "terraces",
                flush=True,
            )
            if len(cases) == 1 and repeats:
                timings.extend(benchmark(layers(plan), row["id"], repeats))
    if not cases:
        raise ValueError(
            "No actual stepped source has a complete usable near and outside product"
        )
    report = revision() | dict(
        status="pass",
        cases=cases,
        rejected=rejected,
        receipts=receipts,
        timings=timings,
        negative_controls=controls,
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        scope="Bounded actual source generic stepped controls with shared contours, core/rooms/terraces/night fixture IDs; source use programmes, continuous native player, global family/LOD/frame/device admission remain open",
    )
    (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/stepped-source"),
    )
    parser.add_argument("--sites", nargs="+", default=list(SITES))
    parser.add_argument("--repeats", type=int, default=21)
    args = parser.parse_args()
    run(args.output, args.sites, args.repeats)
