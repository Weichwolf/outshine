"""Actual MVT body/headroom through existing diagnostic Buildings/Vegetation handoff."""

import argparse
import hashlib
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from compare_stair_envelopes import run as construct
from mvt_body_occupancy import body_occupancy
from outshine_ref.contracts import (
    BuildingFootprint,
    HeightInterval,
    LocalFrame,
    SourceFeature,
)
from stair_module_inputs import reference_profiles
from stair_occupancy_experiment import save_volumes
from stair_occupancy_handoff import OccupancyQuery, adjusted
from stair_occupancy_pipeline import handoff
from stair_occupancy_verify import read


def diagnostic_probes(data, ids, clearances):
    vertices, probes = data["body_vertices"], []
    for face, (top, bottom, mask) in enumerate(
        zip(data["top_faces"], data["bottom_faces"], data["face_sources"], strict=True)
    ):
        upper, lower = vertices[top], vertices[bottom]
        clearance = max(
            clearances[key] for i, key in enumerate(ids) if int(mask) & (1 << i)
        )
        common_low, common_high = lower[:, 2].max(), upper[:, 2].min() + clearance
        if common_low >= common_high:
            raise ValueError("diagnostic complete-face interior band does not exist")
        middle = (common_low + common_high) * 0.5
        delta = min(0.1, (common_high - common_low) * 0.25)
        bands = (
            ("below", lower[:, 2].min() - 1, lower[:, 2].min() - 0.5),
            ("inside", middle - delta, middle + delta),
            (
                "above",
                upper[:, 2].max() + clearance + 0.5,
                upper[:, 2].max() + clearance + 1,
            ),
        )
        for kind, lo, hi in bands:
            probes.append(
                BuildingFootprint(
                    f"diagnostic:{face}:{kind}",
                    shapely.Polygon(upper[:, :2]),
                    HeightInterval(float(lo), float(hi)),
                    (face,),
                    {"diagnostic": True, "band": kind},
                )
            )
    return tuple(probes)


def consume(plan, data, ids, rows, probes):
    query = OccupancyQuery(rows)
    changes, notes = adjusted(query, probes)
    if any(note.get("deferred") for note in notes):
        raise ValueError("diagnostic footprint handoff unresolved")
    bounds = data["body_vertices"][:, :2].max(axis=0) + 1
    outside = BuildingFootprint(
        "diagnostic:outside",
        shapely.box(*bounds, *(bounds + 0.25)),
        HeightInterval(0, 1),
        attributes={"diagnostic": True},
    )
    transport = tuple(
        SourceFeature(
            key,
            shapely.LineString(plan["source_metadata"][key]["xy_m"]),
            plan["source_metadata"][key],
        )
        for key in ids
    )
    frame = LocalFrame(json.dumps(plan["coordinate_frame"], sort_keys=True))
    world, seen = handoff(
        frame, probes, rows, changes, (*probes, outside), transport_sources=transport
    )
    kept = {p.key: p for p in world.footprints}
    states = []
    for original, note in zip(probes, notes, strict=True):
        actual = kept.get(original.key)
        expected = original.attributes["band"] != "inside"
        if bool(actual) != expected or (
            actual is not None
            and (
                not actual.geometry.equals(original.geometry)
                or actual.attributes != original.attributes
                or actual.heights != original.heights
            )
        ):
            raise ValueError(
                "actual body/clearance band or retained provenance differs"
            )
        states.append(
            {
                "id": original.source_id,
                "part": original.part,
                "heights_m": [original.heights.lower_m, original.heights.upper_m],
                "original_wkb": original.geometry.wkb_hex,
                "retained_wkb": None if actual is None else actual.geometry.wkb_hex,
                "band": original.attributes["band"],
            }
        )
    if seen["probe_blocked"] != (True,) * len(probes) + (False,):
        raise ValueError("Transport→Buildings→Vegetation diagnostic order differs")
    return {
        "probe_states": states,
        "plant_blocked": seen["probe_blocked"],
        "source_ids": [s.source_id for s in world.sources.transport],
        "building_query_work": {
            key: sum(note[key] for note in notes)
            for key in (
                "query_rows",
                "xy_candidates",
                "height_candidates",
                "positive_conflicts",
            )
        },
        "scope": "588 whole-cap diagnostic building proposals and589 vegetation probes including one outside point. Callbacks reserve retained footprint prisms; they emit no architectural buildings or plants and are not original provider buildings.",
    }


def run(method, precision, trace):
    plan, data, products, source = construct("envelope_indexed_band_remesh", False)
    data = dict(data)
    if precision == "float32":
        data["body_vertices"] = data["body_vertices"].astype("f4").astype(float)
    ids = tuple(row["source_id"] for row in plan["rows"])
    recipes = reference_profiles()
    clearances = {
        key: recipes[plan["source_metadata"][key]["kind"]]["clearanceM"] for key in ids
    }
    probes = diagnostic_probes(data, ids, clearances)
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    rows, owners, records, work = body_occupancy(
        data, ids, clearances, owner_id="diagnostic-native-body", method=method
    )
    prepared = time.perf_counter()
    consumers = consume(plan, data, ids, rows, probes)
    finished = time.perf_counter()
    memory = {}
    if trace:
        _current, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        memory["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report = {
        "method": method,
        "precision": precision,
        "instrumented": trace,
        "prepare_s": prepared - began,
        "consume_s": finished - prepared,
        "complete_handoff_s": finished - began,
        "work": work,
        "source_clearances_m": clearances,
        "coordinate_frame": plan["coordinate_frame"],
        "source_metadata": plan["source_metadata"],
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        **memory,
        "source_body_report": source,
        "scope": "Only occupancy preparation and the complete diagnostic handoff are timed/traced, after identical source/fit/body/products/decoded-precision and probe preparation. Serialization/reload/image follow measurement. GEOS native heap/copies are not fully traced; RSS is whole-process. No hot-path, cache/runtime, locomotion or native/device/Place admission.",
    }
    return data, products, rows, owners, records, consumers, report


def display(data, rows, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    figure = plt.figure(figsize=(12, 6))
    vertices = data["body_vertices"]
    body = vertices[data["body_faces"]]
    upper = []
    for row in rows:
        xy = np.array(row.geometry.exterior.coords[:-1])
        a, b, c = row.upper_plane_m
        ox, oy = row.origin_xy_m
        upper.append(np.c_[xy, a + b * (xy[:, 0] - ox) + c * (xy[:, 1] - oy)])
    for index in (0, 1):
        axis = figure.add_subplot(1, 2, index + 1, projection="3d")
        axis.add_collection3d(
            Poly3DCollection(
                body,
                facecolors="#708b65",
                edgecolors=(0, 0, 0, 0.08),
                linewidths=0.2,
                alpha=0.8,
            )
        )
        if index:
            axis.add_collection3d(
                Poly3DCollection(
                    upper,
                    facecolors="#d19e51",
                    edgecolors=(0, 0, 0, 0.08),
                    linewidths=0.2,
                    alpha=0.25,
                )
            )
        low = body.min(axis=(0, 1))
        high = np.maximum(body.max(axis=(0, 1)), np.array(upper).max(axis=(0, 1)))
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2], high[2]),
            xlabel="east / m",
            ylabel="north / m",
            zlabel="height / m",
            title="Unchanged full local body"
            if not index
            else "Actual caps + existing 2.5 m profile clearance",
        )
        axis.set_box_aspect(high - low)
        axis.view_init(elev=26, azim=-55)
    figure.suptitle(
        "Generator-only spatial handoff; no character or outside-domain admission"
    )
    figure.tight_layout()
    figure.savefig(output / "body-headroom.png", dpi=150, bbox_inches="tight")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--method", choices=("faces", "sources"), default="faces")
    parser.add_argument(
        "--precision", choices=("float32", "float64"), default="float32"
    )
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    data, products, rows, owners, records, consumers, report = run(
        args.method, args.precision, args.trace_memory
    )
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **data)
    (args.output / "shared-products.json").write_text(
        json.dumps(products, indent=2) + "\n"
    )
    (args.output / "consumer-products.json").write_text(
        json.dumps(consumers, indent=2) + "\n"
    )
    (args.output / "face-origins.json").write_text(json.dumps(records, indent=2) + "\n")
    report["volumes"] = save_volumes(args.output, rows, owners)
    decoded, _records = read(args.output / "volumes.json", report)
    ids = tuple(report["source_clearances_m"])
    probes = diagnostic_probes(data, ids, report["source_clearances_m"])
    decoded_products = consume(
        {
            "source_metadata": report["source_metadata"],
            "coordinate_frame": report["coordinate_frame"],
        },
        data,
        ids,
        decoded,
        probes,
    )
    if decoded_products != consumers:
        raise ValueError("offline product roundtrip changed consumer decisions")
    report["offline_roundtrip_exact"] = True
    report["consumer_semantic_sha256"] = hashlib.sha256(
        json.dumps(
            {k: v for k, v in consumers.items() if k != "building_query_work"},
            sort_keys=True,
        ).encode()
    ).hexdigest()
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(data, rows, args.output)
    print(
        json.dumps(
            {
                "method": args.method,
                "prepare_s": report["prepare_s"],
                "consume_s": report["consume_s"],
                "consumer_semantic_sha256": report["consumer_semantic_sha256"],
            }
        )
    )


if __name__ == "__main__":
    main()
