"""Compare whole-tile and component assembly on identical captured source inputs."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from transport_component_surfaces import assemble
from transport_flat_plan import plan
from transport_flat_surfaces import solve
from transport_network import source_identity, transport_widths
from transport_travel import compile_travel

ROOT = Path(__file__).resolve().parents[3]


def prepared(site, base):
    path = base / "inputs" / (site + "-input.json")
    source = json.loads(path.read_text())
    roads = [
        {
            "line": shapely.LineString(r["coordinates"]),
            "width": r["width_m"],
            "properties": r["properties"],
            "source_id": r["source_id"],
        }
        for r in source["roads"]
    ]
    recipes = json.loads(
        (ROOT / "test/experiments/transport/transport_network_recipes.json").read_text()
    )
    accepted, graphs, usage = plan(roads, recipes, transport_widths())
    travel = compile_travel(accepted, graphs, recipes["travelRecipes"])
    widths = np.array([r["width"] for r in accepted])
    for graph in graphs.values():
        graph["half_widths"] = (
            np.maximum.reduceat(
                widths[graph["owner_sources"]], graph["owner_offsets"][:-1]
            )
            * 0.5
        )
    return source, roads, accepted, list(graphs.values()), usage, travel


def assemble_global(graphs):
    try:
        product, audit = solve(graphs, junction_mode="cuts", corner_radius=2.0)
        if (
            audit["ports"]["invalid_sections"]
            or audit["cuts"]["unresolved_cuts"]
            or audit["cuts"]["remaining_cut_conflicts"]
        ):
            raise ValueError("unresolved_global_ports_or_cuts")
        if len(product["indices"]) > 262144:
            raise ValueError("global_output_face_budget")
    except (ValueError, shapely.errors.GEOSException) as failure:
        return [], {"complete": False, "preparation_reason": str(failure)}
    return [product], {"complete": True, "audit": audit}


def ledger(roads, accepted, usage, products, report, method):
    assignments = {}
    if method == "global":
        for index in range(len(accepted)):
            assignments[index] = (
                "" if products else report.get("preparation_reason", "global_failure")
            )
    else:
        for record in report.get("components", ()):
            for index in record["source_indices"]:
                if index in assignments and assignments[index] != record["reason"]:
                    raise ValueError(
                        "one source spans inconsistent physical components"
                    )
                assignments[index] = record["reason"]
    keys = {road["source_key"]: index for index, road in enumerate(accepted)}
    coverage = shapely.union_all(
        [
            shapely.union_all(shapely.polygons(p["vertices"][p["indices"]]))
            for p in products
        ]
    )
    merged = {r["id"]: r for r in usage["merged_junction_sources"]}
    rows = []
    for old in usage["usage"]:
        row = dict(old)
        if old["status"] != "not_used":
            key = source_identity(roads[old["source"]])
            index = keys.get(key)
            if index is not None:
                reason = assignments.get(
                    index, report.get("preparation_reason", "source_without_surface")
                )
            elif row["id"] in merged:
                point = shapely.Point(merged[row["id"]]["position_m"])
                reason = (
                    "" if coverage.covers(point) else "merged_contact_without_surface"
                )
            else:
                reason = "source_without_surface_binding"
            if reason:
                row.update(status="not_used", reason=reason)
        rows.append(row)
    assert len(rows) == len(roads)
    assert all(r["id"] == roads[r["source"]]["source_id"] for r in rows)
    assert all(r["reason"] for r in rows if r["status"] == "not_used")
    return rows


def store_graphs(graphs, accepted, output):
    arrays = {}
    for index, graph in enumerate(graphs):
        for key in (
            "vertices",
            "edges",
            "half_widths",
            "owner_offsets",
            "owner_sources",
            "owner_directions",
            "travel_public",
            "travel_restricted",
        ):
            arrays[f"{index}_{key}"] = graph[key]
        xy, owners = shapely.get_coordinates(graph["lines"], return_index=True)
        arrays[f"{index}_line_coordinates"] = xy
        arrays[f"{index}_line_offsets"] = np.r_[
            0, np.cumsum(np.bincount(owners, minlength=len(graph["lines"])))
        ]
    np.savez_compressed(output / "graph.npz", **arrays)
    (output / "source-ids.json").write_text(
        json.dumps([r["source_id"] for r in accepted]) + "\n"
    )


def worker(site, method, base):
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    output = base / "results" / site / method
    output.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    source, roads, accepted, graphs, usage, travel = prepared(site, base)
    prepared_ms = (time.perf_counter() - began) * 1000
    cpu_before = resource.getrusage(resource.RUSAGE_SELF)
    began = time.perf_counter()
    if method == "global":
        products, report = assemble_global(graphs)
    else:
        products, report = assemble(graphs, prefer_global=method != "serial")
    assembly_ms = (time.perf_counter() - began) * 1000
    cpu_after = resource.getrusage(resource.RUSAGE_SELF)
    rows = ledger(roads, accepted, usage, products, report, method)
    (output / "source-usage.json").write_text(json.dumps(rows, indent=2) + "\n")
    store_graphs(graphs, accepted, output)
    shapes = []
    for index, product in enumerate(products):
        np.savez_compressed(output / f"product-{index}.npz", **product)
        shapes.append(
            shapely.union_all(shapely.polygons(product["vertices"][product["indices"]]))
        )
    domain = shapely.union_all(shapes)
    (output / "domain.wkb").write_bytes(domain.wkb)
    result = {
        "site": site,
        "method": method,
        "input_sha256": hashlib.sha256(
            (base / "inputs" / (site + "-input.json")).read_bytes()
        ).hexdigest(),
        "original_features": len(source["original_features"]),
        "deferred_polygons": len(source["deferred_sources"]),
        "input_axes": len(roads),
        "planned_axes": len(accepted),
        "normalization": usage["normalization"],
        "source_states": dict(Counter(r["status"] for r in rows)),
        "source_reasons": dict(
            Counter(r["reason"] for r in rows if r["status"] == "not_used")
        ),
        "travel": travel,
        "products": len(products),
        "faces": sum(len(p["indices"]) for p in products),
        "area_m2": float(domain.area),
        "preparation_ms": prepared_ms,
        "assembly_ms": assembly_ms,
        "assembly_cpu_ms": (
            cpu_after.ru_utime
            + cpu_after.ru_stime
            - cpu_before.ru_utime
            - cpu_before.ru_stime
        )
        * 1000,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "report": report,
    }
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "site",
                    "method",
                    "products",
                    "source_states",
                    "assembly_ms",
                    "peak_rss_bytes",
                )
            }
        ),
        flush=True,
    )


def compare(base):
    inventory = json.loads((base / "inventory.json").read_text())
    # Start every worker before the large independent geometry comparison phase;
    # fork/exec must not inherit a previous case's parent-side RSS high-water mark.
    for site, _, _ in inventory["sites"]:
        for method in ("global", "serial", "components"):
            subprocess.run(
                [
                    sys.executable,
                    __file__,
                    "--data-root",
                    str(base),
                    "--worker",
                    site,
                    method,
                ],
                check=True,
                timeout=90,
            )
    rows = []
    for site, _, _ in inventory["sites"]:
        paths = [base / "results" / site / m for m in ("global", "components")]
        before, after = [json.loads((p / "result.json").read_text()) for p in paths]
        serial_path = base / "results" / site / "serial"
        serial = json.loads((serial_path / "result.json").read_text())
        assert (
            before["input_sha256"] == after["input_sha256"]
            and before["travel"] == after["travel"]
        )
        with np.load(paths[0] / "graph.npz") as a, np.load(paths[1] / "graph.npz") as b:
            assert a.files == b.files and all(
                np.array_equal(a[k], b[k]) for k in a.files
            )
        domains = [shapely.from_wkb((p / "domain.wkb").read_bytes()) for p in paths]
        difference = float(domains[0].symmetric_difference(domains[1]).area)
        if before["report"]["complete"] and after["report"]["complete"]:
            assert difference <= max(1e-7, domains[0].area * 1e-12), (site, difference)
        serial_domain = shapely.from_wkb((serial_path / "domain.wkb").read_bytes())
        serial_difference = float(domains[0].symmetric_difference(serial_domain).area)
        if before["report"]["complete"] and serial["report"]["complete"]:
            assert serial_difference <= max(1e-7, domains[0].area * 1e-12)
        assert serial["input_sha256"] == before["input_sha256"]
        assert serial["travel"] == before["travel"]
        rows.append(
            {
                "site": site,
                "before": before,
                "after": after,
                "domain_difference_m2": difference,
                "input_graph_masks_identical": True,
                "serial": serial,
                "serial_domain_difference_m2": serial_difference,
            }
        )
    output = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)
        ),
        "cases": rows,
    }
    (base / "comparison.json").write_text(json.dumps(output, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--worker", nargs=2, metavar=("SITE", "METHOD"))
    args = parser.parse_args()
    if args.worker:
        worker(*args.worker, args.data_root.resolve())
    else:
        compare(args.data_root.resolve())


if __name__ == "__main__":
    main()
