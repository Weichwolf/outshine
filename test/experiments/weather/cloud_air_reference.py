"""Independent homogeneous-air / procedural-cloud controls for the native prefix."""

import argparse
import gzip
import json
from itertools import pairwise
from pathlib import Path

import numpy as np
from cloud_field import Layer, density, segments, sphere_roots
from cloud_radiance import source

RAY = np.dtype(
    [("ray", "<f4", 5), ("begin", "<u4"), ("end", "<u4"), ("azimuth", "<f4")]
)
SAMPLE = np.dtype(
    [
        ("radiance", "<f4", 3),
        ("through", "<f4", 3),
        ("sources", "<u4"),
        ("counts", "<u4"),
    ]
)
assert RAY.itemsize == 32 and SAMPLE.itemsize == 32


def direction_for(ray):
    cosine = float(ray["ray"][1])
    azimuth = float(ray["azimuth"])
    horizontal = np.sqrt(max(0.0, 1.0 - cosine * cosine))
    return np.array(
        [horizontal * np.cos(azimuth), cosine, horizontal * np.sin(azimuth)]
    )


def origin_for(ray, bottom_km):
    # The supplied AirRay is binary32; preserve its native metre conversion.
    height = np.float32(np.float32(ray["ray"][0] - bottom_km) * 1000.0)
    return np.array([0.0, float(height), 0.0])


def make_inputs(output, bottom_km, subset):
    records, queries = [], []
    eyes = {
        "all": (1772.0, 2800.0, 4000.0, 110000.0),
        "ground": (1772.0, 2800.0, 4000.0),
        "orbit": (110000.0,),
    }[subset]
    for eye in eyes:
        for cosine in (-1.0, -0.1, -0.025, 0.0, 0.1, 1.0):
            for azimuth in (0.0, np.pi, np.pi * 0.5):
                ray = np.zeros((), dtype=RAY)
                extent_km = 400.0 if cosine == -0.025 else 200.0
                ray["ray"] = [bottom_km + eye * 0.001, cosine, 0.8, 0.0, extent_km]
                ray["azimuth"] = azimuth
                ray["ray"][3] = np.dot(direction_for(ray), [-0.6, 0.8, 0.0])
                ray["begin"] = len(queries)
                distances = np.r_[
                    0.0, 0.0, np.geomspace(1e-5, extent_km, 64), extent_km
                ]
                queries.extend(distances)
                ray["end"] = len(queries)
                records.append(ray)
    rays = np.array(records, dtype=RAY)
    distances = np.array(queries, dtype="<f4")
    rays.tofile(output / "rays.bin")
    distances.tofile(output / "queries.bin")
    return rays, distances


def coefficients(ray, medium, metadata):
    cosine = float(ray["ray"][3])
    rayleigh_phase = 3.0 * (1.0 + cosine * cosine) / (16.0 * np.pi)
    g = float(medium[14])
    mie_phase = (
        3.0
        * (1.0 - g * g)
        * (1.0 + cosine * cosine)
        / (8.0 * np.pi * (2.0 + g * g) * (1.0 + g * g - 2.0 * g * cosine) ** 1.5)
    )
    rayleigh = medium[4:7].astype(float)
    mie_scattering, mie_extinction = float(medium[7]), float(medium[11])
    sunlight = np.asarray(metadata["stored_sunlight_rgb"])
    psi = np.asarray(metadata["stored_psi_rgb"])
    return (
        (rayleigh + mie_extinction) * 0.001,
        (
            sunlight * (rayleigh * rayleigh_phase + mie_scattering * mie_phase)
            + (rayleigh + mie_scattering) * psi
        )
        * 0.001,
    )


def cloud_cells(
    layer,
    origin,
    direction,
    extent_m,
    warp=1,
    view_samples=16,
    light_samples=6,
    light_gauss3=False,
    seconds=37.0,
):
    cells = []
    for start, end in segments(origin, direction, layer):
        end = min(end, extent_m)
        if end <= start:
            continue
        for tap in range(view_samples):
            begin = start + (end - start) * (tap / view_samples) ** warp
            finish = start + (end - start) * ((tap + 1) / view_samples) ** warp
            at = origin + direction * (0.5 * (begin + finish))
            extinction = float(density(at, layer, seconds))
            emission = extinction * source(
                at, direction, layer, seconds, light_samples, light_gauss3
            )
            cells.append((begin, finish, extinction, emission))
    return cells


def clear_span(origin, direction, extent_m, bottom_m, top_m):
    near, far, hit = sphere_roots(origin, direction, top_m - bottom_m, bottom_m)
    if not hit or far <= 0:
        return 0.0, 0.0
    begin, end = max(0.0, near), min(extent_m, far)
    ground_near, _, ground_hit = sphere_roots(origin, direction, 0.0, bottom_m)
    inward = np.dot(origin, direction) + bottom_m * direction[1] < 0
    if ground_hit and inward and ground_near >= 0:
        end = min(end, ground_near)
    return (begin, end) if end > begin else (0.0, 0.0)


def reference_ray(
    ray,
    distances,
    medium,
    metadata,
    cover,
    warp,
    view_samples=16,
    light_samples=6,
    light_gauss3=False,
):
    bottom_m, top_m = float(medium[0]) * 1000, float(medium[1]) * 1000
    origin, direction = origin_for(ray, medium[0]), direction_for(ray)
    layer = Layer(radius_m=bottom_m, cover=cover)
    extent_m = float(ray["ray"][4]) * 1000
    cells = (
        cloud_cells(
            layer,
            origin,
            direction,
            extent_m,
            warp,
            view_samples,
            light_samples,
            light_gauss3,
        )
        if cover > 0
        else []
    )
    air_begin, air_end = clear_span(origin, direction, extent_m, bottom_m, top_m)
    air_extinction, air_emission = coefficients(ray, medium, metadata)
    edges = sorted(
        set([0.0, extent_m, air_begin, air_end] + [v for c in cells for v in c[:2]])
    )
    slabs = []
    for begin, end in pairwise(edges):
        middle = 0.5 * (begin + end)
        extinction, emission = np.zeros(3), np.zeros(3)
        if air_begin <= middle < air_end:
            extinction += air_extinction
            emission += air_emission
        for cloud_begin, cloud_end, cloud_extinction, cloud_emission in cells:
            if cloud_begin <= middle < cloud_end:
                extinction += cloud_extinction
                emission += cloud_emission
        slabs.append((begin, end, extinction, emission))
    results, counts = [], []
    for distance in distances:
        query_m = float(distance) * 1000
        radiance, through = np.zeros(3), np.ones(3)
        for begin, end, extinction, emission in slabs:
            width = max(0.0, min(query_m, end) - begin)
            opacity = -np.expm1(-width * extinction)
            local = np.divide(
                emission * opacity,
                extinction,
                out=emission * width,
                where=extinction > 0,
            )
            radiance += through * local
            through *= np.exp(-width * extinction)
        results.append(np.r_[radiance, through])
        counts.append(sum(c[0] < query_m for c in cells))
    return np.array(results), np.array(counts)


def prepare(
    output,
    fixtures,
    cover,
    subset,
    warp=1,
    view_samples=16,
    light_samples=6,
    light_gauss3=False,
):
    medium = np.fromfile(fixtures / "medium.bin", dtype="<f4")
    if medium[2] < 1e19 or medium[3] < 1e19 or np.any(medium[8:11] != 0):
        raise ValueError(
            "this independent reference requires the homogeneous-air fixture"
        )
    with gzip.open(fixtures / "rays.json.gz", "rt") as stream:
        metadata = json.load(stream)
    output.mkdir(parents=True, exist_ok=True)
    rays, distances = make_inputs(output, medium[0], subset)
    references, density_counts = [], []
    for ray in rays:
        begin, end = int(ray["begin"]), int(ray["end"])
        reference, counts = reference_ray(
            ray,
            distances[begin:end],
            medium,
            metadata,
            cover,
            warp,
            view_samples,
            light_samples,
            light_gauss3,
        )
        references.append(reference)
        density_counts.extend(counts)
    np.savez(
        output / "reference.npz",
        values=np.concatenate(references),
        density_counts=np.array(density_counts),
    )
    return {
        "rays": len(rays),
        "queries": len(distances),
        "cover": cover,
        "warp": warp,
        "view_samples": view_samples,
        "light_samples": light_samples,
        "light_gauss3": light_gauss3,
        "scope": "Homogeneous finite clear air, actual procedural cloud cells and sources; no general LUT/field convergence.",
    }


def check(output, samples_path, cover, view_samples=16, light_samples=6):
    samples = np.fromfile(samples_path, dtype=SAMPLE)
    references = np.load(output / "reference.npz")
    measured = np.concatenate([samples["radiance"], samples["through"]], axis=1)
    errors = np.abs(measured - references["values"])
    invalid = samples["counts"] & 255
    density_counts = (samples["counts"] >> 16) & 255
    sources = (samples["counts"] >> 8) & 255
    report = {
        "samples": len(samples),
        "max_radiance_error": float(np.max(errors[:, :3])),
        "max_transmittance_error": float(np.max(errors[:, 3:])),
        "finite": bool(np.isfinite(measured).all()),
        "invalid_queries": int(np.sum(invalid)),
        "max_cloud_density_evaluations": int(density_counts.max()),
        "max_cloud_source_evaluations": int(sources.max()),
        "max_clear_source_evaluations": int(samples["sources"].max()),
        "max_committed_union_cells": int((samples["counts"] >> 24).max()),
        "cloud_density_work_upper_bound": int(
            np.max(density_counts + 2 * light_samples * sources)
        ),
        "density_counts_match": bool(
            np.array_equal(density_counts, references["density_counts"])
        ),
        "nonnegative": bool(np.all(measured >= 0)),
        "bounded_transmittance": bool(np.all(samples["through"] <= 1)),
    }
    radiance_limit, through_limit = (1e-5, 2e-6) if cover <= 0 else (1e-4, 1e-4)
    report["radiance_limit"] = radiance_limit
    report["transmittance_limit"] = through_limit
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    assert (
        report["finite"] and report["nonnegative"] and report["bounded_transmittance"]
    )
    assert report["invalid_queries"] == 0 and report["density_counts_match"]
    assert report["max_cloud_density_evaluations"] <= 2 * view_samples
    assert report["max_cloud_source_evaluations"] <= 2 * view_samples
    assert report["max_clear_source_evaluations"] <= 63
    assert report["max_committed_union_cells"] <= 2 * view_samples + 36
    assert (
        report["max_radiance_error"] < radiance_limit
        and report["max_transmittance_error"] < through_limit
    )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--cover", type=float, default=0.65)
    parser.add_argument("--samples", type=Path)
    parser.add_argument("--subset", choices=("all", "ground", "orbit"), default="all")
    parser.add_argument("--warp", type=int, choices=(1, 2), default=1)
    parser.add_argument("--view-samples", type=int, default=16)
    parser.add_argument("--light-samples", type=int, default=6)
    parser.add_argument("--light-gauss3", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.view_samples <= 64 or not 1 <= args.light_samples <= 32:
        parser.error("require view samples in [1,64] and light samples in [1,32]")
    if args.light_gauss3 and args.light_samples != 3:
        parser.error("Gauss3 requires exactly three light samples")
    print(
        json.dumps(
            prepare(
                args.output,
                args.fixtures,
                args.cover,
                args.subset,
                args.warp,
                args.view_samples,
                args.light_samples,
                args.light_gauss3,
            )
        )
    )
    if args.samples:
        print(
            json.dumps(
                check(
                    args.output,
                    args.samples,
                    args.cover,
                    args.view_samples,
                    args.light_samples,
                )
            )
        )


if __name__ == "__main__":
    main()
