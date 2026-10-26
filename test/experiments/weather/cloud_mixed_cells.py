"""Compare finite cloud-event quadrature with locally combined medium coefficients."""

import argparse
import json
import re
from pathlib import Path

import numpy as np
from cloud_field import Layer, altitude
from cloud_moments import centroid_fraction, field_cells


def opacity(depth):
    return -np.expm1(-depth)


def local_transfer(air_depth, cloud_depth, air_source, cloud_source):
    """Independent closed-form RTE for a homogeneous overlapping medium cell."""
    depth = air_depth + cloud_depth
    emitted = air_depth * air_source + cloud_depth * cloud_source
    radiance = np.divide(
        emitted * opacity(depth), depth, out=np.zeros_like(emitted), where=depth > 0
    )
    return radiance, np.exp(-depth)


def cloud_events(air_depth, cloud_depth, air_source, cloud_source, nodes):
    """Integrate the first-cloud event with an exact finite clear-air prefix."""
    cloud_opacity = opacity(cloud_depth)
    if nodes == 1:
        positions = centroid_fraction(cloud_depth)[..., None]
        weights = np.ones(1)
    else:
        points, weights = np.polynomial.legendre.leggauss(nodes)
        points, weights = (points + 1) * 0.5, weights * 0.5
        positions = np.divide(
            -np.log1p(-cloud_opacity[..., None] * points),
            cloud_depth[..., None],
            out=np.broadcast_to(points, (*cloud_depth.shape, nodes)).copy(),
            where=cloud_depth[..., None] > 0,
        )
    air_through = np.exp(-air_depth[..., None] * positions)
    source = (1 - air_through) * air_source[..., None]
    source += air_through * cloud_source[..., None]
    events = cloud_opacity * np.sum(source * weights, axis=-1)
    return np.exp(-cloud_depth) * opacity(air_depth) * air_source + events


def endpoint_reconstruction(air_depth, cloud_depth, clear_radiance, cloud_source):
    """Infer one homogeneous air source from its endpoint transfer; not general."""
    depth = air_depth + cloud_depth
    air_scale = np.divide(
        air_depth * opacity(depth),
        depth * opacity(air_depth),
        out=np.divide(
            opacity(cloud_depth),
            cloud_depth,
            out=np.ones_like(cloud_depth),
            where=cloud_depth > 0,
        ),
        where=air_depth > 0,
    )
    cloud_scale = np.divide(
        cloud_depth * opacity(depth),
        depth,
        out=np.zeros_like(depth),
        where=depth > 0,
    )
    return clear_radiance * air_scale + cloud_source * cloud_scale


def variable_source_counterexample():
    """Same clear endpoint, different source order inside a cloudy cell."""
    air_half, cloud_half = np.array([0.5]), np.array([1.5])
    front, back = np.array([1.0]), np.array([0.0])
    clear_first, clear_t = local_transfer(air_half, back, front, back)
    clear_last, _ = local_transfer(air_half, back, back, back)
    clear = clear_first + clear_t * clear_last
    joint_first, joint_t = local_transfer(air_half, cloud_half, front, back)
    joint_last, _ = local_transfer(air_half, cloud_half, back, back)
    exact = joint_first + joint_t * joint_last
    endpoint = endpoint_reconstruction(2 * air_half, 2 * cloud_half, clear, back)
    return {
        "clear_radiance": float(clear[0]),
        "joint_exact": float(exact[0]),
        "endpoint_reconstruction": float(endpoint[0]),
        "endpoint_absolute_error": float(abs(endpoint[0] - exact[0])),
    }


def compare_depths(air, cloud):
    # Both independent source basis vectors are necessary to bound their mixture.
    air = np.tile(air, 2)
    cloud = np.tile(cloud, 2)
    air_source = np.repeat([0.0, 1.0], len(air) // 2)
    cloud_source = 1.0 - air_source
    exact, _ = local_transfer(air, cloud, air_source, cloud_source)
    methods = {}
    for nodes in (1, 2, 4, 8):
        measured = cloud_events(air, cloud, air_source, cloud_source, nodes)
        error = np.abs(measured - exact)
        worst = int(np.argmax(error))
        methods[str(nodes)] = {
            "max_absolute_radiance_error": float(error[worst]),
            "worst_air_depth": float(air[worst]),
            "worst_cloud_depth": float(cloud[worst]),
            "clear_prefix_queries_per_cloud_cell": nodes + 1,
            "cloud_source_evaluations_per_cloud_cell": 1,
            "error_air_depth_at_most_one": float(np.max(error[air <= 1.0])),
        }
    return methods


def declared_fields(root):
    body = (root / "src/world/sky/Atmosphere.h").read_text()
    body = body.split("inline constexpr Medium kEarthAir = {", 1)[1].split("};", 1)[0]

    def value(name):
        match = re.search(r"\." + name + r"\s*=\s*(\{[^}]+\}|[^,]+)", body)
        if match is None:
            raise ValueError(f"missing native medium field {name}")
        return np.array(
            [
                float(v.strip().removesuffix("f"))
                for v in match[1].strip("{}").split(",")
            ]
        )

    elevations, azimuths = np.meshgrid(
        np.deg2rad([-5, -1, 0, 0.05, 1, 5, 30, 90]), np.arange(4) * np.pi * 0.5
    )
    directions = np.stack(
        [
            np.cos(elevations) * np.sin(azimuths),
            np.sin(elevations),
            np.cos(elevations) * np.cos(azimuths),
        ],
        axis=-1,
    ).reshape(-1, 3)
    air_depths, cloud_depths = [], []
    for eye_m in (0.0, 1772.0, 2800.0, 4000.0):
        origin = np.array([0.0, eye_m, 0.0])
        for cover in (0.2, 0.65, 1.0):
            layer = Layer(cover=cover)
            begin, width, cloud = field_cells(origin, directions, layer, 16)
            points = origin + (begin + 0.5 * width)[..., None] * directions[:, None]
            height = np.maximum(0.0, altitude(points, layer.radius_m)) * 0.001
            rayleigh = np.exp(
                -height[..., None] / value("RayleighScaleHeightKm")
            ) * value("RayleighScatteringPerKm")
            mie = np.exp(-height[..., None] / value("MieScaleHeightKm")) * value(
                "MieExtinctionPerKm"
            )
            ozone = np.maximum(
                0.0,
                1.0
                - np.abs(height[..., None] - value("OzoneCentreKm"))
                / value("OzoneHalfWidthKm"),
            ) * value("OzoneAbsorptionPerKm")
            for haze in (0.0, 1.0, 4.0):
                air = (rayleigh + haze * mie + ozone) * width[..., None] * 0.001
                air_depths.append(air.ravel())
                cloud_depths.append(np.repeat(cloud[..., None], 3, axis=-1).ravel())
    air, cloud = np.concatenate(air_depths), np.concatenate(cloud_depths)
    return {
        "geometry_rays": 4 * 3 * len(directions),
        "density_samples_per_ray_at_most": 32,
        "haze_comparison_values": [0.0, 1.0, 4.0],
        "max_cell_air_depth": float(np.max(air)),
        "max_cell_cloud_depth": float(np.max(cloud)),
        "methods": compare_depths(air, cloud),
        "scope": "Actual declared field depths; constant independent source basis per cell.",
    }


def compare(root):
    depths = np.r_[0.0, np.geomspace(1e-10, 100.0, 64)]
    air, cloud = np.meshgrid(depths, depths)
    air, cloud = air.ravel(), cloud.ravel()
    endpoint = endpoint_reconstruction(air, cloud, opacity(air), np.zeros_like(cloud))
    exact, _ = local_transfer(air, cloud, np.ones_like(air), np.zeros_like(cloud))
    endpoint_error = float(np.max(np.abs(endpoint - exact)))
    assert endpoint_error < 1e-14
    counterexample = variable_source_counterexample()
    assert counterexample["endpoint_absolute_error"] > 0.05
    # A local coefficient sum preserves the independent homogeneous RTE exactly.
    # Real clear-source discontinuities must still split the joint partition.
    return {
        "homogeneous_source_basis_cases": 2 * len(air),
        "first_cloud_event": compare_depths(air, cloud),
        "declared_fields": declared_fields(root),
        "endpoint_homogeneous_max_error": endpoint_error,
        "endpoint_variable_source_counterexample": counterexample,
        "decision": "combine coefficients on the union of clear and cloud cells",
        "limits": "No general variable-field, native, image or frame-budget proof.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[3]
    )
    args = parser.parse_args()
    result = compare(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
