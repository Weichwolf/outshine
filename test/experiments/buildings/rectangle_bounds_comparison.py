"""Pinned offline footprints: rectangle parity, cost and unchanged scope geometry."""

import argparse
import hashlib
import json
import subprocess
import tempfile
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.buildings._rectangle import minimum_rectangle
from outshine_ref.buildings.modelling import Shape, scope_bbox
from samples import read_site


def legacy(commit):
    path = "lib/src/outshine_ref/buildings/modelling/scope_bbox.py"
    raw = subprocess.check_output(["git", "show", commit + ":" + path])
    expected = (
        raw.decode()
        .replace(
            "from .extrude import euler",
            "from .._rectangle import minimum_rectangle\nfrom .extrude import euler",
            1,
        )
        .replace(
            "rectangle = shapely.minimum_rotated_rectangle(shapely.multipoints(points)).normalize()",
            "rectangle = minimum_rectangle(shapely.multipoints(points), points).normalize()",
            1,
        )
    )
    if Path(scope_bbox.__file__).read_text() != expected:
        raise ValueError(
            "baseline scope body differs beyond the measured rectangle call"
        )

    def old(shape, selector):
        with patch.object(
            scope_bbox,
            "minimum_rectangle",
            lambda geometry, _coordinates: shapely.minimum_rotated_rectangle(geometry),
        ):
            return shape.align_scope_to_geometry_bbox(selector)

    return old, hashlib.sha256(raw).hexdigest()


def dimensions(rectangle):
    p = np.asarray(rectangle.exterior.coords)[:4]
    return np.sort(np.linalg.norm(p[1:3] - p[:2], axis=1))


def scope_pair(polygon, baseline):
    source = Shape.from_polygon(polygon, 3).extrude(7)
    before, after = (
        baseline(source, "xz").emit(),
        source.align_scope_to_geometry_bbox("xz").emit(),
    )
    if before.keys() != after.keys():
        raise ValueError("scope emitted fields changed")
    maximum = 0
    for key in before:
        a, b = np.asarray(before[key]), np.asarray(after[key])
        if a.dtype.kind in "fc":
            np.testing.assert_allclose(a, b, atol=3e-8, rtol=0)
            maximum = max(maximum, float(abs(a - b).max(initial=0)))
        else:
            np.testing.assert_array_equal(a, b)
    return before, after, maximum


def plot(pair, label, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig = plt.figure(figsize=(12.8, 7.2), layout="constrained")
    for index, mesh in enumerate(pair):
        axis = fig.add_subplot(1, 2, index + 1, projection="3d")
        p = mesh["vertices"][:, [0, 2, 1]] * [1, -1, 1]
        axis.plot_trisurf(
            *p.T,
            triangles=mesh["faces"],
            color="#86b7b5",
            linewidth=0.2,
            edgecolor="#314a50",
        )
        axis.set(
            xlabel="East [m]",
            ylabel="North [m]",
            zlabel="Up [m]",
            title=("Original GEOS scope", "Known rectangle reuse")[index],
        )
        axis.set_box_aspect(np.maximum(np.ptp(p, axis=0), 1))
        axis.view_init(elev=26, azim=-55)
    fig.suptitle(
        label
        + "\nSame original footprint; declared 3m base / 7m extrusion, diagnostic geometry"
    )
    fig.savefig(output / "scope-parity.png", dpi=100)
    plt.close(fig)


def run(args):
    baseline, baseline_sha = legacy(args.baseline)
    report = {
        "baseline": args.baseline,
        "baseline_scope_sha256": baseline_sha,
        "sites": [],
        "changed_rectangles": [],
    }
    display = None
    for site in args.sites:
        rows, input_receipt = read_site(site)
        old_times, new_times = [], []
        changed = metrics_changed = 0
        maximum_metric = maximum_emitted = 0
        coordinate_bytes = 0
        for index, row in enumerate(rows):
            polygon = row["polygon"]
            coordinates = shapely.get_coordinates(polygon)
            old = new = None
            for mode in (0, 1) if index % 2 else (1, 0):
                began = time.perf_counter_ns()
                value = (
                    minimum_rectangle(polygon, coordinates)
                    if mode
                    else shapely.minimum_rotated_rectangle(polygon)
                )
                elapsed = time.perf_counter_ns() - began
                if mode:
                    new = value
                    new_times.append(elapsed)
                else:
                    old = value
                    old_times.append(elapsed)
            coordinate_bytes += coordinates.nbytes
            delta = abs(dimensions(old) - dimensions(new))
            metrics_changed += bool(np.any(delta))
            maximum_metric = max(maximum_metric, float(delta.max()))
            equal = old.normalize().equals_exact(new.normalize(), 0)
            if not equal:
                changed += 1
                # Independent backend hull: any changed answer must attain the
                # exact rectangular hull, rather than relax an arbitrary shape.
                if not polygon.convex_hull.normalize().equals_exact(new.normalize(), 0):
                    raise ValueError("unproven rectangle change")
                before, after, emitted = scope_pair(polygon, baseline)
                maximum_emitted = max(maximum_emitted, emitted)
                report["changed_rectangles"].append(
                    {
                        "source_id": row["id"],
                        "source_sha256": row["source_sha256"],
                        "source_wkb": polygon.wkb_hex,
                        "old": np.asarray(old.exterior.coords).tolist(),
                        "new": np.asarray(new.exterior.coords).tolist(),
                        "dimension_difference_m": delta.tolist(),
                        "maximum_emitted_difference_m": emitted,
                    }
                )
                if display is None:
                    display = ((before, after), row["id"])
        report["sites"].append(
            {
                "site": site,
                "pinned_source_receipt": input_receipt["receipt"],
                "source_access": "offline hash-checked cache; receipt acquisition fields are historical",
                "footprints": len(rows),
                "exact_normalized_rectangle_changes": changed,
                "dimension_changes": metrics_changed,
                "maximum_dimension_difference_m": maximum_metric,
                "maximum_emitted_difference_m": maximum_emitted,
                "existing_coordinate_array_bytes_borrowed": coordinate_bytes,
                "old_new_p50_ns": [float(np.median(t)) for t in (old_times, new_times)],
                "old_new_p95_ns": [
                    float(np.percentile(t, 95)) for t in (old_times, new_times)
                ],
            }
        )
        print(site, report["sites"][-1], flush=True)
    if display:
        plot(display[0], display[1], args.output)
    report["scope"] = (
        "Pinned offline source footprints, source IDs retained; no provider request or native/world/device admission. Nonidentical rectangle answers must equal the exact source hull; source mesh/payload stationarity uses the unchanged existing3e-8 scope bound. Coordinate copies and single-pass alternating call times counted, not total GEOS allocations/peak. Diagnostic renderer is Matplotlib."
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--sites", nargs="+", default=["Vienna", "Tokyo", "Rosenheim"])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    if not args.output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    run(args)
