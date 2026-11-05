"""Compare lazy shared LOD trees with independent generation on captured road recipes."""

import argparse
import hashlib
import json
import lzma
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from road_surface_levels import products
from road_surface_lod import prepare
from road_surface_model import Curve


def compare(source, errors):
    curve = Curve(source)
    began = time.process_time()
    shared = list(products(curve, errors))
    shared_s = time.process_time() - began
    began = time.process_time()
    independent = [prepare(curve, error) for error in errors]
    independent_s = time.process_time() - began
    for candidate, reference in zip(shared, independent, strict=True):
        for key in ("stations_m", "positions_enh_m", "indices"):
            if candidate[key].tobytes() != reference[key].tobytes():
                raise ValueError(
                    "shared hierarchy changed an independently generated product"
                )
        if candidate["maximum_stored_bound_m"] != reference["maximum_stored_bound_m"]:
            raise ValueError("shared hierarchy changed the existing surface bound")
    return {
        "shared_cpu_s": shared_s,
        "independent_cpu_s": independent_s,
        "shared_queries": shared[-1]["cumulative_envelope_pose_tests"],
        "independent_queries": sum(plan["envelope_pose_tests"] for plan in independent),
        "levels": [
            {
                "error_m": error,
                "triangles": len(plan["indices"]),
                "stored_bound_m": plan["maximum_stored_bound_m"],
                "new_queries": plan["envelope_pose_tests"],
                "reused_intervals": plan["reused_intervals"],
                "array_bytes": sum(
                    plan[key].nbytes
                    for key in ("stations_m", "positions_enh_m", "indices")
                ),
            }
            for error, plan in zip(errors, shared, strict=True)
        ],
    }


def plot(source, errors, output, ordinal):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    curve = Curve(source)
    plans = list(products(curve, errors))
    reference = curve.at(np.linspace(*source["range_m"], 1001))
    figure = plt.figure(figsize=(5 * len(plans), 5), layout="constrained")
    for at, (plan, error) in enumerate(zip(plans, errors, strict=True)):
        axes = figure.add_subplot(1, len(plans), at + 1, projection="3d")
        points = plan["positions_enh_m"].reshape(-1, 3)
        axes.plot_trisurf(
            *points.T,
            triangles=plan["indices"],
            color="lightgrey",
            edgecolor="black",
            linewidth=0.3,
            shade=True,
        )
        for edge in (0, 3):
            axes.plot(*reference[:, edge].T, color="crimson", linewidth=0.7)
        axes.view_init(elev=42, azim=-65)
        span = np.ptp(reference.reshape(-1, 3), axis=0)
        axes.set_box_aspect(np.maximum(span, max(span) * 0.1))
        axes.set(
            title=f"{error:g} m: {len(plan['indices'])} triangles",
            xlabel="East (m)",
            ylabel="North (m)",
            zlabel="Up (m)",
        )
    figure.suptitle(
        f"Source {ordinal}: analytic Upper products; red = sampled source edges\n"
        "No ports, closed-body, normal, World or device admission"
    )
    figure.savefig(output / f"surface-levels-{ordinal}.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--errors-m", type=float, nargs="+", default=[16, 4, 1, 0.25])
    parser.add_argument("--plot-ordinal", type=int)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    sources = [
        Path(__file__).with_name(name)
        for name in (
            "road_surface_levels.py",
            "road_surface_lod.py",
            "road_surface_model.py",
            "compare_road_surface_levels.py",
        )
    ]
    sources.append(root / "lib/src/outshine_ref/transport/clothoid_turns.py")
    hashes = {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sources
    }
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    output = args.output.resolve()
    if output.is_relative_to(root):
        raise ValueError("write private comparison outputs outside the checkout")
    folder = root / "test/outshine/data/transport/road_curve_requests"
    manifest = json.loads((folder / "manifest.json").read_text())
    archive = folder / manifest["archive"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != manifest["archive_sha256"]:
        raise ValueError("captured source archive changed")
    rows, witness = [], None
    with lzma.open(archive, "rt") as stream:
        for line in stream:
            original = json.loads(line)
            source = {
                key: original[key]
                for key in ("range_m", "section_m", "seams", "intervals")
            }
            result = compare(source, args.errors_m)
            rows.append({"ordinal": original["ordinal"], **result})
            if original["ordinal"] == args.plot_ordinal:
                witness = source
    if len(rows) != manifest["records"]:
        raise ValueError("incomplete original corpus")
    if any(
        hashlib.sha256(path.read_bytes()).hexdigest()
        != hashes[str(path.relative_to(root))]
        for path in sources
    ):
        raise ValueError("producer changed during comparison")
    report = {
        "commit": commit,
        "archive_sha256": manifest["archive_sha256"],
        "platform": platform.platform(),
        "implementation_sha256": hashes,
        "scope": "Whole retained recipe corpus, upper surfaces only. Source acquisition includes legacy Fine fields; only analytic metadata reaches generation. Equal geometry/bounds, no normal/port/body/World/device admission. Single ordered Python CPU pass; not an isolated performance benchmark, allocation/copy/peak costs unmeasured.",
        "errors_m": args.errors_m,
        "rows": rows,
        "totals": {
            key: sum(row[key] for row in rows)
            for key in (
                "shared_cpu_s",
                "independent_cpu_s",
                "shared_queries",
                "independent_queries",
            )
        },
        "triangles_by_level": [
            sum(row["levels"][level]["triangles"] for row in rows)
            for level in range(len(args.errors_m))
        ],
    }
    output.mkdir(parents=True, exist_ok=True)
    if args.plot_ordinal is not None:
        if witness is None:
            raise ValueError("requested diagnostic source is absent from the corpus")
        plot(witness, args.errors_m, output, args.plot_ordinal)
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("totals", "triangles_by_level")}))


if __name__ == "__main__":
    main()
