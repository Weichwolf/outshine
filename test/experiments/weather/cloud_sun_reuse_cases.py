"""Actual Weather/air ray controls and explicit local-map fixture assembly."""

import struct
from pathlib import Path

import numpy as np
from cloud_air_reference import RAY, direction_for, origin_for
from cloud_air_source_reference import PSI, SOLAR, inputs, medium_parameters
from cloud_field import Layer
from cloud_radiance import SUN
from cloud_sun_reuse import build_field


def ray_controls(sparse=False):
    original, _ = inputs()
    rays = list(original.copy())
    for index in (0, 6, 12, 18):
        ray = original[index].copy()
        ray["ray"][1] = 0.8
        ray["azimuth"] = np.pi
        ray["ray"][3] = np.dot(direction_for(ray), SUN)
        rays.append(ray)
    result, queries = [], []
    for ray in rays:
        maximum = 120 if ray["ray"][0] > 6460 else 20
        distances = np.r_[
            0, 0.001, 0.01, 0.025, 0.05, 0.1, np.linspace(0.25, maximum, 80)
        ]
        if sparse:
            distances = distances[::5]
        ray["begin"] = len(queries)
        queries.extend(np.repeat(distances.astype("<f4"), 2))
        ray["end"] = len(queries)
        result.append(ray)
    return np.array(result, RAY), np.array(queries, "<f4")


def write_inputs(output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    medium_parameters().tofile(output / "medium.bin")
    for name, rgb in (("veil", SOLAR), ("multi", PSI)):
        pixels = np.tile([*rgb, 1], (2, 2, 1)).astype("<f2")
        (output / f"{name}.bin").write_bytes(
            struct.pack("<2I", 2, 2) + pixels.tobytes()
        )
    for sparse in (False, True):
        rays, queries = ray_controls(sparse)
        suffix = "-sparse" if sparse else ""
        rays.tofile(output / f"rays{suffix}.bin")
        queries.tofile(output / f"queries{suffix}.bin")


def write_maps(output, rays, cover, dimension=513):
    medium = medium_parameters()
    keys = list(dict.fromkeys(float(ray["ray"][0]) for ray in rays))
    fields = [
        build_field(
            Layer(cover=cover),
            SUN,
            37,
            origin_for(
                next(ray for ray in rays if float(ray["ray"][0]) == key), medium[0]
            ),
            dimension=dimension,
        )
        for key in keys
    ]
    atlas = np.empty((len(fields) * dimension, 3 * dimension), dtype="<f4")
    records = []
    for scene, field in enumerate(fields):
        for level, data in enumerate(field.levels):
            x, y = level * dimension, scene * dimension
            atlas[y : y + dimension, x : x + dimension] = data.values
            values = [*data.origin_uv, *data.step_uv]
            scalar = [format(float(np.float32(value)), ".9g") for value in values]
            records.append(
                f"SourceSolarMap(vec2({','.join(scalar[:2])}), vec2({','.join(scalar[2:])}), "
                f"ivec2({x},{y}), ivec2({dimension},{dimension}))"
            )
    (output / "solar.bin").write_bytes(
        struct.pack("<2I", atlas.shape[1], atlas.shape[0]) + atlas.tobytes()
    )
    scenes = [keys.index(float(ray["ray"][0])) for ray in rays]
    declaration = (
        "struct SourceSolarMap { vec2 OriginUv; vec2 StepUv; ivec2 AtlasOrigin; ivec2 Extent; };\n"
        f"const SourceSolarMap sourceSolarMaps[{len(records)}] = SourceSolarMap[{len(records)}](\n"
        + ",\n".join(records)
        + ");\n"
        + f"const uint sourceSolarScenes[{len(scenes)}] = uint[{len(scenes)}]("
        + ",".join(f"{value}u" for value in scenes)
        + ");\n"
        + "uint sourceSolarScene(uint index) { return sourceSolarScenes[index]; }\n"
        + f"const ivec2 sourceSolarAtlasExtent = ivec2({atlas.shape[1]},{atlas.shape[0]});\n"
    )
    (output / "source_sun_maps.glsl").write_text(declaration)
    return fields, scenes, atlas.nbytes
