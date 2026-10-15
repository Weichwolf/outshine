"""Full-size numerical image and independent witnesses; not an engine frame gate."""

import ctypes
import ctypes.util
import json

import numpy as np
from cloud_air_reference import RAY, SAMPLE
from cloud_air_source_reference import medium_parameters, reference_ray


def load_snapshot():
    path = ctypes.util.find_library("nvidia-ml")
    if not path:
        return {"available": False}
    try:
        library = ctypes.CDLL(path)
        if library.nvmlInit_v2() != 0:
            return {"available": False}
        handle = ctypes.c_void_p()

        class Utilization(ctypes.Structure):
            _fields_ = [("gpu", ctypes.c_uint), ("memory", ctypes.c_uint)]

        class Memory(ctypes.Structure):
            _fields_ = [
                ("total", ctypes.c_ulonglong),
                ("free", ctypes.c_ulonglong),
                ("used", ctypes.c_ulonglong),
            ]

        try:
            value, memory = Utilization(), Memory()
            valid = library.nvmlDeviceGetHandleByIndex_v2(0, ctypes.byref(handle)) == 0
            valid = (
                valid
                and library.nvmlDeviceGetUtilizationRates(handle, ctypes.byref(value))
                == 0
            )
            valid = (
                valid
                and library.nvmlDeviceGetMemoryInfo(handle, ctypes.byref(memory)) == 0
            )
            return {
                "available": valid,
                "gpu_percent": value.gpu,
                "memory_percent": value.memory,
                "used_device_bytes": memory.used,
                "total_device_bytes": memory.total,
            }
        finally:
            library.nvmlShutdown()
    except (OSError, AttributeError):
        return {"available": False}


def image_rays(width, height):
    tangent = np.tan(np.deg2rad(55) / 2)
    x = (np.arange(width, dtype="f4") + 0.5) * 2 / width - 1
    y = 1 - (np.arange(height, dtype="f4") + 0.5) * 2 / height
    xx, yy = np.meshgrid(x, y)
    directions = np.stack(
        [-np.ones((height, width)), yy * tangent, -xx * tangent * width / height],
        axis=-1,
    ).astype("f4")
    directions /= np.linalg.norm(directions, axis=-1)[..., None]
    directions = directions.reshape(-1, 3)
    count = width * height
    rays = np.zeros(count, RAY)
    rays["ray"][:, 0] = np.float32(6361.772)
    rays["ray"][:, 1] = directions[:, 1]
    rays["ray"][:, 2] = 0.8
    rays["ray"][:, 3] = directions @ np.array([-0.6, 0.8, 0], dtype="f4")
    rays["ray"][:, 4] = 10000
    rays["azimuth"] = np.arctan2(directions[:, 2], directions[:, 0])
    rays["begin"] = np.arange(count, dtype="u4")
    rays["end"] = rays["begin"] + 1
    return rays, np.full(count, 10, dtype="f4")


def independent_witnesses(rays, direct, reused, differences, output):
    count = len(rays)
    chosen = np.unique(
        np.r_[np.argsort(differences)[-8:], np.linspace(0, count - 1, 12, dtype=int)]
    )
    reports = []
    for index in chosen:
        original = reference_ray(
            rays[index], np.array([10.0]), medium_parameters(), 0.65, 16
        )[0]
        refined = reference_ray(
            rays[index], np.array([10.0]), medium_parameters(), 0.65, 32
        )[0]
        delta = float(np.max(abs(original - refined)))
        if delta > 1e-8:
            raise RuntimeError("independent full-image witness is not converged")
        reports.append(
            {
                "index": int(index),
                "reference_refinement_max_abs": delta,
                "direct_relative": float(
                    np.max(
                        abs(direct["radiance"][index] - refined[:3])
                        / np.maximum(abs(refined[:3]), 1e-5)
                    )
                ),
                "reuse_relative": float(
                    np.max(
                        abs(reused["radiance"][index] - refined[:3])
                        / np.maximum(abs(refined[:3]), 1e-5)
                    )
                ),
                "reference_RGB": refined[:3].tolist(),
            }
        )
    (output / "independent-witnesses.json").write_text(json.dumps(reports, indent=2))
    return reports


def benchmark(args, binary, execute, compile_probe, gpu_times):
    width, height = 1920, 1080
    output = args.output / "full-1920x1080"
    output.mkdir(exist_ok=True)
    case = args.output / "cover-0.65"
    rays, queries = image_rays(width, height)
    rays.tofile(output / "rays.bin")
    queries.tofile(output / "queries.bin")
    metadata = (
        (case / "source_sun_maps.glsl")
        .read_text()
        .replace("return sourceSolarScenes[index];", "return 1u;")
    )
    (output / "source_sun_maps.glsl").write_text(metadata)
    profiles = []
    for name, reuse in (("direct", 0), ("reuse", 1)):
        shader = compile_probe(args, output, name, reuse, 0, execute, 0.65)
        before = load_snapshot()
        log = execute(
            [
                binary,
                shader,
                output / "rays.bin",
                output / "queries.bin",
                *(args.output / f"{n}.bin" for n in ("veil", "multi", "medium")),
                output / f"{name}.bin",
                args.repeats,
                "--source-map",
                case / "solar.bin",
                "--gpu-timing",
            ],
            output,
            f"{name}-capture",
        )
        times = gpu_times(log)
        profiles.append(
            {
                "profile": name,
                "dispatch_ms": times,
                "warm_median_ms": float(np.median(times[1:] or times)),
                "load_before": before,
                "load_after": load_snapshot(),
            }
        )
    direct = np.memmap(output / "direct.bin", dtype=SAMPLE, mode="r")
    reused = np.memmap(output / "reuse.bin", dtype=SAMPLE, mode="r")
    for captured in (direct, reused):
        values = np.c_[captured["radiance"], captured["through"]]
        if (
            not np.isfinite(values).all()
            or np.any(values < 0)
            or np.any(captured["through"] > 1)
            or np.any(captured["counts"] & 0x80000000)
            or np.any(captured["sources"] > 531)
        ):
            raise RuntimeError("invalid full-image transfer or work bound")
    error = np.max(
        abs(direct["radiance"] - reused["radiance"])
        / np.maximum(abs(direct["radiance"]), 1e-5),
        axis=1,
    )
    witness = independent_witnesses(rays, direct, reused, error, output)
    record = {
        "width": width,
        "height": height,
        "query_km": 10,
        "profiles": profiles,
        "relative_RGB_change_max": float(error.max()),
        "relative_RGB_change_p99": float(np.quantile(error, 0.99)),
        "transmittance_bitwise": direct["through"].tobytes()
        == reused["through"].tobytes(),
        "independent_witnesses": witness,
        "accepted_runtime_profile": False,
        "scope": "Isolated homogeneous-control compute image; map GPU generation, engine/Places and frame budget are unproven.",
    }
    (output / "report.json").write_text(json.dumps(record, indent=2))
    print(
        json.dumps(
            {
                "benchmark_profiles": profiles,
                "RGB_change_max": float(error.max()),
                "witness_max": max(x["reuse_relative"] for x in witness),
            }
        ),
        flush=True,
    )
    return record
