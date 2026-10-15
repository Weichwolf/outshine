"""Actual small-source roof envelopes, independent composition, sections and build costs."""

import argparse
import itertools
import json
import time
from collections import Counter
from pathlib import Path

import manifold3d as manifold
import numpy as np
import shapely
from evidence import revision
from outshine_ref.buildings.modelling import (
    PrismVolume,
    heightfield_envelope,
    union_prisms,
)
from outshine_ref.buildings.structures import classify_structure
from provider_contract import attributes
from structural_shell_geometry import mesh_section, native
from tile_buildings import SITES, read_tile
from tile_roof_envelopes import choose, roof_surface


def measures(envelope):
    triangles = envelope.vertex_roots[envelope.mesh["faces"]]
    edges = np.concatenate(
        [triangles[:, [0, 1]], triangles[:, [1, 2]], triangles[:, [2, 0]]]
    )
    _, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    np.testing.assert_array_equal(counts, 2)
    np.testing.assert_array_equal(
        np.bincount(inverse, weights=np.where(edges[:, 0] < edges[:, 1], 1, -1)), 0
    )
    solid = native(envelope.mesh, envelope.vertex_roots)
    if abs(solid.volume() - envelope.volume_m3) > 1e-7:
        raise ValueError(
            "Actual solid volume differs from its exact upper-surface integral"
        )
    return solid


def composed(row, parameters, choice, wedge):
    body = union_prisms(
        [
            PrismVolume(
                row["id"] + ":lower",
                shapely.from_wkb(row["polygon"].wkb),
                parameters["minHeight"],
                choice.eave_m,
            )
        ]
    )
    result = manifold.Manifold.batch_boolean(
        [native(body.mesh, body.vertex_roots), native(wedge)], manifold.OpType.Add
    )
    if result.status() != manifold.Error.NoError:
        raise ValueError(
            "Independent body/roof composition rejected: " + str(result.status())
        )
    return result


def sections(envelope, other):
    heights = np.unique(envelope.mesh["vertices"][:, 2])
    maximum = 0.0
    # Native CrossSection quantization was measured on actual ENU input at
    # 1/10/1000 units per metre. Millimetre units reduce its boundary error;
    # all comparisons and the unchanged 1e-7 area threshold remain in metres.
    units_per_m = 1000.0
    section_solid = other.scale((units_per_m,) * 3)
    for a, b in itertools.pairwise(heights):
        height = (a + b) / 2
        actual = mesh_section(envelope.mesh, envelope.vertex_roots, height)
        values = section_solid.slice(height * units_per_m).to_polygons()
        expected = shapely.Polygon()
        polygons = [
            shapely.Polygon(np.asarray(value) / units_per_m) for value in values
        ]
        for polygon in sorted(polygons, key=lambda p: -p.area):
            expected = expected.symmetric_difference(polygon)
        maximum = max(maximum, actual.symmetric_difference(expected).area)
    if maximum > 1e-7:
        raise ValueError(
            "Complete heightfield and independent composition sections differ"
        )
    return {"slabs": len(heights) - 1, "maximum_section_error_m2": maximum}


def case(row, parameters, choice, compare=False):
    began = time.perf_counter()
    top, wedge, _ = roof_surface(row, choice)
    surface_ms = (time.perf_counter() - began) * 1000
    began = time.perf_counter()
    envelope = heightfield_envelope(top, parameters["minHeight"])
    envelope_ms = (time.perf_counter() - began) * 1000
    solid = measures(envelope)
    domain = shapely.from_wkb(envelope.footprint_wkb)
    if domain.symmetric_difference(row["polygon"]).area > 1e-7:
        raise ValueError("Envelope domain changed its source footprint")
    if not np.array_equal(
        [envelope.mesh["vertices"][:, 2].min(), envelope.mesh["vertices"][:, 2].max()],
        [parameters["minHeight"], parameters["buildingHeight"]],
    ):
        raise ValueError("Envelope changed the source height interval")
    result = {
        "id": row["id"],
        "kind": choice.kind,
        "rise_m": choice.rise_m,
        "surface_ms": surface_ms,
        "envelope_ms": envelope_ms,
        "triangles": len(envelope.mesh["faces"]),
        "volume_m3": solid.volume(),
        "area_m2": solid.surface_area(),
        "status": "pass",
    }
    if compare:
        began = time.perf_counter()
        try:
            other = composed(row, parameters, choice, wedge)
            np.testing.assert_allclose(
                [other.volume(), other.surface_area()],
                [solid.volume(), solid.surface_area()],
                atol=1e-7,
                rtol=1e-9,
            )
            xor = (other - solid).volume() + (solid - other).volume()
            if abs(xor) > 1e-7:
                raise ValueError(
                    "Independent composition has different occupied volume"
                )
            result.update(
                comparison_status="pass",
                symmetric_volume_error_m3=float(xor),
                **sections(envelope, other),
            )
        except (ValueError, AssertionError) as error:
            result.update(comparison_status="rejected", comparison_reason=str(error))
        result["composition_ms"] = (time.perf_counter() - began) * 1000
    return result, envelope, top, wedge


def benchmark(row, parameters, choice, repeats):
    top, wedge, _ = roof_surface(row, choice)
    values = {"direct": [], "composition": []}
    for index in range(repeats + 2):
        for name, samples in values.items():
            began = time.perf_counter()
            heightfield_envelope(
                top, parameters["minHeight"]
            ) if name == "direct" else composed(row, parameters, choice, wedge)
            if index >= 2:
                samples.append((time.perf_counter() - began) * 1000)
    return {
        "id": row["id"],
        "phases": {
            name: {
                "samples": repeats,
                "p50_ms": float(np.percentile(data, 50)),
                "p95_ms": float(np.percentile(data, 95)),
            }
            for name, data in values.items()
        },
        "scope": "Same prepared roof and closed source volume, direct boundary completion versus native body/roof union; host Python build work, no native frame claim",
    }


def run(output, repeats):
    output.mkdir(parents=True, exist_ok=True)
    results = []
    rejected = []
    receipts = []
    selected = []
    compared = set()
    for site in SITES:
        rows, receipt = read_tile(site)
        receipts.append(receipt["receipt"])
        counts = Counter()
        for row in rows:
            if row["layer"] != "building":
                continue
            try:
                parameters = attributes(
                    {
                        k: v
                        for k, v in row["properties"].items()
                        if k not in ("colour", "color", "building:colour")
                    }
                )
            except ValueError:
                continue
            if parameters["hide3d"]:
                continue
            if (
                classify_structure(
                    row["polygon"],
                    row["properties"],
                    coordinate_step_m=row["coordinate_step_m"],
                    clipped=row["clipped"],
                ).kind
                != "ordinary"
            ):
                continue
            choice = choose(row, parameters)
            if choice.kind == "flat":
                continue
            try:
                comparison = (site, choice.kind) not in compared
                if comparison:
                    compared.add((site, choice.kind))
                result, _envelope, _top, _wedge = case(
                    row, parameters, choice, comparison
                )
                results.append(result)
                counts[choice.kind] += 1
                if comparison and result["comparison_status"] == "pass":
                    selected.append((row, parameters, choice))
            except (
                ValueError,
                NotImplementedError,
                shapely.errors.GEOSException,
            ) as error:
                rejected.append(
                    {"id": row["id"], "kind": choice.kind, "reason": str(error)}
                )
        print(
            json.dumps(
                {
                    "site": site,
                    "complete": dict(counts),
                    "rejected": sum(r["id"].startswith(site + "/") for r in rejected),
                }
            ),
            flush=True,
        )
    if len(results) < 450 or len(rejected) > 5:
        raise ValueError(
            "Small-source roof coverage fell below its explicit prototype regression scope"
        )
    timings = (
        [benchmark(row, p, c, repeats) for row, p, c in selected[:2]] if repeats else []
    )
    (output / "verification.json").write_text(
        json.dumps(
            revision()
            | {
                "status": "pass",
                "cases": results,
                "rejected": rejected,
                "receipts": receipts,
                "timings": timings,
                "scope": "Actual provider exterior prototypes; direct topology/volume, bounded independent compositions and height slices. Specific use/room programmes, global classification accuracy and native world/device admission unproved",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/roof-envelopes"),
    )
    parser.add_argument("--repeats", type=int, default=21)
    args = parser.parse_args()
    run(args.output, args.repeats)
