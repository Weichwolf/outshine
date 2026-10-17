"""Equal corner-contour comparison; complete room/mesh admission stays separate."""

import argparse
import gc
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from building_layout import frame
from evidence import revision
from stepped_form_profiles import requests
from stepped_profile_probe import sources


def direct(polygon, axes, distances):
    origin = np.asarray(polygon.centroid.coords[0])
    xy = (shapely.get_coordinates(polygon) - origin) @ axes.T
    low, high = xy.min(axis=0), xy.max(axis=0)
    for a in (-1, 1):
        for b in (-1, 1):
            contours = []
            signs = np.array([a, b])
            for distance in distances:
                if distance == 0:
                    contours.append(polygon)
                    continue
                lo = low + np.where(signs < 0, distance, 0)
                hi = high - np.where(signs > 0, distance, 0)
                vertices = np.array(
                    [[lo[0], lo[1]], [hi[0], lo[1]], [hi[0], hi[1]], [lo[0], hi[1]]]
                )
                contours.append(
                    polygon.intersection(shapely.Polygon(vertices @ axes + origin))
                )
            yield contours


def candidates(polygon, storeys, method):
    transform, _ = frame(polygon)
    axes = np.array([transform[:2], transform[2:4]])
    output = []
    for width in (1.2, 1.8, 2.8):
        for tiers in (1, 2):
            distances = [0.0] * (storeys - tiers) + [
                width * (i + 1) for i in range(tiers)
            ]
            if method == "two-halfplanes":
                output.extend(
                    row[3]
                    for row in requests(
                        {"footprint": polygon}, axes, distances, "corner"
                    )
                )
            else:
                output.extend(direct(polygon, axes, distances))
    return output


def benchmark():
    records = []
    for source, storeys in zip(sources(), (5, 6, 7, 7), strict=True):
        polygon = source["polygon"]
        reference = candidates(polygon, storeys, "two-halfplanes")
        for method in ("two-halfplanes", "direct-box"):
            actual = candidates(polygon, storeys, method)
            maximum = max(
                a.symmetric_difference(b).area
                for x, y in zip(reference, actual, strict=True)
                for a, b in zip(x, y, strict=True)
            )
            if maximum > 1e-7:
                raise ValueError("Contour candidate changed source-domain geometry")
            times = []
            for _ in range(21):
                start = time.perf_counter()
                candidates(polygon, storeys, method)
                times.append((time.perf_counter() - start) * 1000)
            gc.collect()
            tracemalloc.start()
            actual = candidates(polygon, storeys, method)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            records.append(
                {
                    "source_id": source["id"],
                    "method": method,
                    "contour_alternatives": len(actual),
                    "known_boolean_intersections": 72 + 24 * storeys
                    if method == "two-halfplanes"
                    else 36,
                    "result_coordinate_bytes": sum(
                        shapely.get_coordinates(g).nbytes for row in actual for g in row
                    ),
                    "runs_ms": times,
                    "median_ms": float(np.median(times)),
                    "traced_peak_bytes": peak,
                    "maximum_symmetric_difference_m2": maximum,
                    "scope": "Equal requested contours only; Python tracemalloc excludes GEOS heap. Coordinate bytes count result observations, including shared source polygons, not unique storage or all copies. Direct-box has no complete room/portal/3D admission.",
                }
            )
    return records


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(revision() | {"cases": benchmark()}, indent=2) + "\n"
    )
