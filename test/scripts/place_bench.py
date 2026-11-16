"""The Places table: native scenario runs, warm cache, three-run medians."""

import json
import math
import platform
import statistics
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

from place_turn import turn_scenario

PROFILES = {"rtx3060": (1920, 1080, 60), "iris": (1280, 720, 30),
            "a18": (1280, 720, 30)}
CASES = (("Still", 300), ("Drehung", 300), ("Flug", None))
ERRORS = (OSError, ValueError, KeyError, TypeError, RuntimeError,
          subprocess.TimeoutExpired, ET.ParseError)


def median(values):
    return statistics.median(values) if values and all(v is not None for v in values) else None


def number(value, budget=None):
    if value is None:
        return "n/v"
    text = f"{value:.2f}"
    return f"**{text}**" if budget is not None and value > budget else text


def startup_and_peak(reports, gpu=False):
    windows = []
    for report in reports:
        if gpu:
            if report["gpu_times"]["frame"] is None:
                return {"startup_p99_ms": None, "max_ms": None, "worst_run": None,
                        "worst_frame": None}
            samples = sorted(report["gpu_times"]["samples"], key=lambda s: s["submission"])
            values = [s["elapsed_ms"] for s in samples]
        else:
            values = [s["frame_ms"] for s in report["cpu_times"]["samples"]]
        if not values or any(not math.isfinite(v) or v < 0 for v in values):
            raise ValueError("frame samples must be finite, nonnegative and nonempty")
        windows.append(values)
    peaks = [max(values) for values in windows]
    peak = median(peaks)
    run = peaks.index(peak)
    # Nearest-rank p99, matching native QuantileOf; all startup frames remain measured.
    startup = [sorted(values[:10])[math.ceil(len(values[:10]) * .99) - 1] for values in windows]
    return {"startup_p99_ms": median(startup), "max_ms": peak, "worst_run": run + 1,
            "worst_frame": windows[run].index(peak) + 1}


def driver_metadata():
    if platform.system() == "Darwin":
        return {"os_build": platform.version(), "devices": []}
    try:
        result = subprocess.run(["vulkaninfo", "--summary"], capture_output=True,
                                text=True, timeout=15, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"error": str(error), "devices": []}
    devices = []
    for line in result.stdout.splitlines():
        if line.strip().startswith("GPU") and line.strip().endswith(":"):
            devices.append({})
        if devices and "=" in line:
            key, value = (part.strip() for part in line.split("=", 1))
            if key in ("deviceName", "driverName", "driverInfo", "driverVersion"):
                devices[-1][key] = value
    return {"devices": devices, "exit_code": result.returncode}


def selected_driver(metadata, gpu, override):
    if override:
        return override
    for device in metadata["devices"]:
        if device.get("deviceName") == gpu:
            return device.get("driverInfo") or device.get("driverVersion") or "n/v"
    return metadata.get("os_build", "n/v")


def declaration(source, profile, turning):
    width, height, rate = PROFILES[profile]
    text, view, angular = turn_scenario(source, 300 if turning else rate)
    root = ET.fromstring(text)
    render = root.find("render")
    render.set("widthPx", str(width))
    render.set("heightPx", str(height))
    render.set("fps", str(rate))
    if not turning:
        views = root.find("views")
        for extra in list(views)[1:]:
            views.remove(extra)
        angular = "0,0,0"
    else:
        angular = ",".join(format(float(v) * rate / 300, ".17g") for v in angular.split(","))
    return ET.tostring(root, encoding="unicode"), view, angular


def forward_velocity(report):
    matrix = report["motion"]["start"]["world_from_camera"]
    if len(matrix) != 16 or not all(math.isfinite(v) for v in matrix):
        raise ValueError("native start camera has no finite model matrix")
    forward = [-matrix[i] for i in (8, 9, 10)]
    length = math.hypot(*forward)
    if length <= 0:
        raise ValueError("native start camera has no forward direction")
    return ",".join(format(100 * v / length, ".17g") for v in forward)


def validate_report(report, options, frames, angular, linear):
    width, height, rate = PROFILES[options.bench_profile]
    expected_gpu = {"rtx3060": "RTX 3060", "iris": "Iris", "a18": "A18"}[options.bench_profile]
    if expected_gpu not in report["gpu"]:
        raise ValueError(f"GPU {report['gpu']} does not match profile {options.bench_profile}")
    if report["backend"] != ("Metal" if options.bench_profile == "a18" else "Vulkan"):
        raise ValueError("native benchmark backend does not match the device profile")
    if report["build"]["commit"] != options.commit:
        raise ValueError("native benchmark build does not match the current clean commit")
    profile, motion = report["profile"], report["motion"]
    if (profile["width_px"], profile["height_px"], profile["fps"]) != (width, height, rate):
        raise ValueError("native benchmark used a different resolution or rate")
    if motion["frames"] != frames or len(report["cpu_times"]["samples"]) != frames:
        raise ValueError("native benchmark used a different measurement window")
    if report["gpu_times"]["frame"] is not None and len(report["gpu_times"]["samples"]) != frames:
        raise ValueError("native benchmark did not time every GPU frame")
    for distribution in (report["cpu_times"]["frame"], report["gpu_times"]["frame"]):
        if distribution is not None and any(not math.isfinite(distribution[k]) or distribution[k] < 0
                                            for k in ("p50_ms", "p99_ms")):
            raise ValueError("native benchmark contains invalid frame times")
    for key, expected in (("angular_velocity_deg_s", angular), ("linear_velocity_mps", linear)):
        if not all(math.isclose(a, float(b), rel_tol=1e-10, abs_tol=1e-10)
                   for a, b in zip(motion[key], expected.split(","), strict=True)):
            raise ValueError("native benchmark used a different motion")
    duration = frames / rate
    for start, finish, velocity in zip(motion["start"]["position_m"],
                                       motion["finish"]["position_m"],
                                       map(float, linear.split(",")), strict=True):
        if not math.isclose(finish, start + duration * velocity, rel_tol=1e-10, abs_tol=1e-6):
            raise ValueError("native camera did not complete the requested path")
    if linear == "0,0,0":
        start = motion["start"]["world_from_camera"]
        finish = motion["finish"]["world_from_camera"]
        if len(start) != 16 or len(finish) != 16 or not all(
            math.isclose(a, b, rel_tol=1e-10, abs_tol=1e-10)
            for a, b in zip(start, finish, strict=True)
        ):
            raise ValueError("still/complete turn did not retain the initial camera pose")


def summarize(reports):
    def field(*path):
        values = []
        for report in reports:
            value = report
            for key in path:
                value = value.get(key) if isinstance(value, dict) else None
            values.append(value)
        return median(values)

    # These existing geometry metrics describe the final submitted frame only.
    metrics = [{m["name"]: m["value"] for m in r["metrics"]} for r in reports]
    image_values = [r.get("image_row_variation") for r in reports]
    image_nonblank = all(v is not None and math.isfinite(v) and v > .1 for v in image_values)
    encoding = []
    for report, metric in zip(reports, metrics, strict=True):
        stages = [value for key, value in metric.items()
                  if key.startswith("cost.stage.") and key.endswith(".total_ms")]
        encoding.append(sum(stages) / report["motion"]["frames"] if stages else None)
    triangles = []
    for metric in metrics:
        keys = [f"{stage}, {kind} input triangles"
                for stage in ("opaqueGeometry", "transmissiveGeometry")
                for kind in ("direct", "indirect")]
        triangles.append(sum(metric[k] for k in keys if k in metric)
                         if any(k in metric for k in keys) else None)
    return {
        "cpu_p50_ms": field("cpu_times", "frame", "p50_ms"),
        "cpu_p99_ms": field("cpu_times", "frame", "p99_ms"),
        "gpu_p50_ms": field("gpu_times", "frame", "p50_ms"),
        "gpu_p99_ms": field("gpu_times", "frame", "p99_ms"),
        "cpu_encoding_ms": median(encoding),
        "cpu_extremes": startup_and_peak(reports),
        "gpu_extremes": startup_and_peak(reports, gpu=True),
        "draws_last": median([m.get("geometry draw calls") for m in metrics]),
        "triangles_last": median(triangles),
        "image_nonblank": image_nonblank,
        "image_row_variation_min": min(image_values) if all(v is not None for v in image_values)
                                   else None,
        "peak_rss_bytes": field("process_memory", "peak_resident_bytes"),
        "gpu_allocated_bytes_after": field("gpu_memory", "resource_allocated_bytes"),
        "warm_load_ms": field("phases_ms", "preload"),
        "gpu_pass_ms": None,
        "complete": len(reports) == 3 and image_nonblank and all(r["measurement_complete"] and r["world_complete"]
                                              and r["unrefined_frames"] == 0 for r in reports),
    }


def run_case(source, case, options, invoke, deadline, seed=None):
    name, frames = case
    if name == "Flug" and seed is None:
        raise ValueError("flight needs a measured native start pose")
    work = options.reports / source.stem / name
    work.mkdir(parents=True, exist_ok=True)
    text, view, angular = declaration(source, options.bench_profile, name == "Drehung")
    prepared = work / "scene.scenario"
    prepared.write_text(text)
    rate = PROFILES[options.bench_profile][2]
    frames = frames or rate * 10
    linear = forward_velocity(seed) if name == "Flug" else "0,0,0"
    reports = []
    # A full untabulated traversal warms the same persistent cache before all three runs.
    for run in range(4):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("benchmark exceeded --max-seconds")
        folder = work / ("warmup" if run == 0 else str(run))
        report = invoke(prepared, view, options, folder, frames, angular, linear,
                        timeout=remaining)
        validate_report(report, options, frames, angular, linear)
        if run:
            reports.append(report)
    identity = [(r["gpu"], r["cpu"], r["backend"], r["build"]["commit"]) for r in reports]
    if len(set(identity)) != 1:
        raise ValueError("device or build changed between repetitions")
    result = summarize(reports)
    result.update({"place": source.stem, "case": name, "frames": frames,
                   "report_paths": [str(work / str(run) / "benchmark.json") for run in (1, 2, 3)],
                   "gpu": reports[0]["gpu"], "cpu": reports[0]["cpu"],
                   "backend": reports[0]["backend"], "commit": reports[0]["build"]["commit"]})
    return result, reports[0]


def table_row(row, budget):
    values = [row["place"], row["case"]]
    values.extend(number(row.get(k), budget) for k in
                  ("cpu_p50_ms", "cpu_p99_ms", "gpu_p50_ms", "gpu_p99_ms"))
    for key in ("cpu_extremes", "gpu_extremes"):
        extreme = row.get(key, {})
        values.append(number(extreme.get("startup_p99_ms"), budget))
        peak = number(extreme.get("max_ms"), budget)
        if extreme.get("worst_frame") is not None:
            peak += f" (L{extreme['worst_run']}/F{extreme['worst_frame']})"
        values.append(peak)
    values.extend(number(row.get(k), limit) for k, limit in
                  (("draws_last", 300), ("triangles_last", 1_500_000)))
    for key in ("peak_rss_bytes", "gpu_allocated_bytes_after"):
        value = row.get(key)
        values.append(number(value / 2**20 if value is not None else None))
    values.extend((number(row.get("warm_load_ms")), "ja" if row["complete"] else "nein",
                   "ja" if row.get("image_nonblank") else "nein",
                   number(row.get("cpu_encoding_ms")), "n/v"))
    return "| " + " | ".join(values) + " |"


def benchmark_places(available, selected, options, invoke):
    began = time.monotonic()
    deadline = began + options.max_seconds
    metadata = driver_metadata()
    width, height, rate = PROFILES[options.bench_profile]
    budget = 1000 / rate
    output = {"profile": options.bench_profile, "width_px": width, "height_px": height,
              "fps": rate, "budget_ms": budget, "runs": 3, "aggregation": "median",
              "hardware": metadata, "rows": [], "elapsed_s": 0}
    options.reports.mkdir(parents=True, exist_ok=True)
    identities = set()
    for name in selected:
        seed = None
        for case in CASES:
            try:
                row, report = run_case(available[name], case, options, invoke, deadline, seed)
                seed = report if case[0] == "Still" else seed
                driver = selected_driver(metadata, row["gpu"], options.driver)
                row["driver"] = driver
                identities.add((row["cpu"], row["gpu"], driver, row["commit"]))
            except ERRORS as error:
                row = {"place": name, "case": case[0], "complete": False, "error": str(error)}
                print(f"{name}/{case[0]}: {error}", file=sys.stderr, flush=True)
            output["rows"].append(row)
            output["elapsed_s"] = time.monotonic() - began
            temporary = options.reports / "results.writing"
            temporary.write_text(json.dumps(output, indent=2, allow_nan=False) + "\n")
            temporary.replace(options.reports / "results.json")
            print(f"{name}/{case[0]}: {'vollständig' if row['complete'] else 'unvollständig'}",
                  file=sys.stderr, flush=True)
    print(f"Profil {options.bench_profile}: {width}×{height}, {rate} fps, {budget:.2f} ms. "
          "Drei Läufe, Median; fett = über Budget. Still/Drehung: 300 Frames, eine volle Drehung; "
          "Flug: zehn Sekunden mit 100 m/s. Anlauf: erste zehn Frames, weiter im Gesamtergebnis. "
          "Max: Median der Laufmaxima mit zugehörigem Lauf/Frame (1-basiert). "
          "Draws/Dreiecke: Geometrie im letzten Frame, ohne Terrain/Schatten. "
          "Bildprüfung: mittlere horizontale RGB-Differenz > 0,1 in allen drei Bildern; "
          "GPU-MiB: Allokation nach Messung, kein Peak. GPU-Passzeiten: noch nicht verfügbar.")
    for cpu, gpu, driver, commit in sorted(identities):
        print(f"CPU: {cpu}; GPU: {gpu}; Treiber: {driver}; Commit: {commit}.")
    print("\n| Place | Lastfall | CPU p50 ms | CPU p99 ms | GPU p50 ms | GPU p99 ms | "
          "CPU Anlauf p99 ms | CPU Max ms (L/F) | GPU Anlauf p99 ms | GPU Max ms (L/F) | "
          "Geometrie-Draws (letzt.) | Geometrie-Dreiecke (letzt.) | Peak-RSS MiB | GPU MiB (danach) | "
          "Laden warm ms | Vollständig | Bildstruktur | CPU-Encoding ms | GPU-Pässe |")
    print("|" + " --- |" * 19)
    for row in output["rows"]:
        print(table_row(row, budget))
    print(f"\nJSON: {options.reports / 'results.json'}; Dauer: {output['elapsed_s']:.1f} s.")
    return int(len(identities) > 1 or any(not r["complete"] or
                   (options.check_budget and (r.get("cpu_p99_ms") is None or
                    r["cpu_p99_ms"] > budget or r.get("gpu_p99_ms") is None or
                    r["gpu_p99_ms"] > budget)) for r in output["rows"]))
