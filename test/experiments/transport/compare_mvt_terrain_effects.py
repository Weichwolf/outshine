"""Compare explicit effects on existing original, two-flight and World bodies."""

import argparse
import gc
import json
import os
import platform
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from mvt_t_profile_products import shared_products
from terrain_source_effects import qualify_effects


def controlled_roles(case):
    """Declared test intent, never an interpretation of provider placement facts."""
    upper = set(
        case["plan"].get("controlled_derivation", {}).get("upper_source_mapping", {})
    )
    recipe = (
        "Controlled support of captured World body parts; no placement inference."
        if "capture" in case
        else "Controlled lower support / upper clearance; no provider-role claim."
    )
    return {
        row["source_id"]: {
            "effect": "OverheadClearance"
            if row["source_id"] in upper
            else "GroundContact",
            "contact_key": 0 if row["source_id"] in upper else 1,
            "clearance_m": 2.0 if row["source_id"] in upper else 0.0,
            "relief_m": 0.0,
            "recipe": recipe,
        }
        for row in case["plan"]["rows"]
    }


def prepare(name):
    if name == "world":
        from world_stair_reference import FIXTURE, construct

        return construct(json.loads(FIXTURE.read_text()))
    if name != "original":
        from mvt_independent_flight_body import construct

        return construct(name, cut_stage="canonical")
    from compare_mvt_native_body import run

    plan, data, _baseline, products, report = run("highs-ds")
    return {
        "plan": plan,
        "data": data,
        "heights": data["reference_vertices"][:, 2],
        "products": products,
        "original_report": report,
    }


def measure(case, declarations, reuse_masks, repeats):
    timings = []
    for _ in range(repeats + 1):
        began = time.perf_counter()
        result = qualify_effects(
            case["plan"], case["data"], declarations, reuse_masks=reuse_masks
        )
        timings.append(time.perf_counter() - began)
        del result
    gc.collect()
    tracemalloc.start()
    result = qualify_effects(
        case["plan"], case["data"], declarations, reuse_masks=reuse_masks
    )
    _current, peak = tracemalloc.get_traced_memory()
    blocks = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
    tracemalloc.stop()
    return result, {
        "first_s": timings[0],
        "following_s": timings[1:],
        "median_s": float(np.median(timings[1:])),
        "python_traced_peak_bytes": peak,
        "python_traced_live_blocks": blocks,
        "work": result["work"],
    }


def export_products(case, declarations, effects, output):
    if "capture" in case:
        records = {
            "terrain-effects": {
                k: v.tolist() if isinstance(v, np.ndarray) else v
                for k, v in effects.items()
            },
            "source-bindings": {
                "captured_source": case["capture"],
                "source_instances_in_mask_bit_order": case["plan"]["rows"],
                "ports": case["ports"],
                "scope": "Body-kernel reference input and explicit part/port bindings; not a shared solver plan, Nav route, terrain-role inference or new product format.",
            },
        }
    else:
        product = shared_products(
            case["plan"],
            case["heights"],
            case["data"],
            terrain_declarations=declarations,
        )
        qualified = product.pop("terrain_effects")
        if product != case["products"]:
            raise ValueError("qualified effects changed an existing consumer")
        product["terrain_effects"] = qualified
        records = {"shared-plan": product}
    for name, value in {"declarations": declarations, **records}.items():
        (output / (name + ".json")).write_text(json.dumps(value, indent=2) + "\n")


def display(case, result, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    data = case["data"]
    points = data["body_vertices"].astype(np.float32).astype(float)
    cap = points[data["bottom_faces"][result["body_cap_face_ids"]]]
    targets = cap.copy()
    targets[:, :, 2] += result["height_offsets_m"][:, None]
    colors = np.where(
        (result["effects"] == 1)[:, None], [0.25, 0.62, 0.32], [0.25, 0.43, 0.85]
    )
    figure = plt.figure(figsize=(13, 6))
    for index, selected in enumerate((cap, targets)):
        axes = figure.add_subplot(1, 2, index + 1, projection="3d")
        axes.add_collection3d(
            Poly3DCollection(
                points[data["top_faces"]],
                facecolors=(0.5, 0.5, 0.5, 0.15),
                edgecolors=(0, 0, 0, 0.08),
                linewidths=0.2,
            )
        )
        axes.add_collection3d(
            Poly3DCollection(
                selected, facecolors=colors, edgecolors=(0, 0, 0, 0.2), linewidths=0.3
            )
        )
        low = np.minimum(points.min(axis=0), targets.min(axis=(0, 1)))
        high = points.max(axis=0)
        axes.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2], high[2]),
            xlabel="east / m",
            ylabel="north / m",
            zlabel="height / m",
            title="Actual body underside"
            if index == 0
            else "Ground targets / clearance ceilings only",
        )
        axes.set_box_aspect(high - low)
        axes.view_init(elev=27, azim=-65)
    figure.suptitle(
        "Controlled explicit roles: green ground, blue clearance; no DEM deformation"
    )
    figure.tight_layout()
    figure.savefig(output / "terrain-effects.png", dpi=150, bbox_inches="tight")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--case", choices=("original", "stacked", "crossed", "world"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=21)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 1000:
        parser.error("bounded positive repeat count required")
    args.output.mkdir(parents=True, exist_ok=True)
    start_load = os.getloadavg()
    began = time.perf_counter()
    case = prepare(args.case)
    preparation = time.perf_counter() - began
    declarations = controlled_roles(case)
    factored, candidate = measure(case, declarations, True, args.repeats)
    direct, baseline = measure(case, declarations, False, args.repeats)
    for key in factored:
        if isinstance(factored[key], np.ndarray):
            np.testing.assert_array_equal(factored[key], direct[key])
        elif key != "work" and factored[key] != direct[key]:
            raise ValueError(f"factoring changed terrain field {key}")
    export_products(case, declarations, factored, args.output)
    np.savez(args.output / "body.npz", **case["data"])
    np.savez(
        args.output / "terrain-effects.npz",
        **{k: v for k, v in factored.items() if isinstance(v, np.ndarray)},
    )
    report = {
        "case": args.case,
        "consumer": "world_body_kernel"
        if "capture" in case
        else "shared_profile_products",
        "body_preparation_s": preparation,
        "factored": candidate,
        "direct": baseline,
        "start_load": start_load,
        "process_peak_rss_bytes_before_plot": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "scope": "Role-only calls after complete body preparation; separate tracing. RSS includes imports and body. Live blocks are not cumulative allocations; no native/World/DEM admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.render:
        display(case, factored, args.output)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
