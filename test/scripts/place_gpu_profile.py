"""Join native GPU diagnostics to the measured submissions, excluding preload and readbacks."""

import json
import math
from pathlib import Path
import statistics

GROUPS = {
    "shadows": {"lightvisibility", "sun shadow", "shadow"},
    "opaque": {"opaquegeometry", "ground", "groundpyramid", "groundselection",
               "geometryrootcull", "geometrycull", "geometryscan", "geometrycompact",
               "depthpyramid"},
    "atmosphere": {"sky", "sun", "moon", "stars", "aerialperspective",
                   "mediumtransmittance", "mediummultiscatter", "mediumradiance",
                   "irradiance", "environmentspecular"},
    "water": {"transmissivegeometry", "compositetransmission"},
    "post": {"temporalresolve", "tonemap", "overlay", "ambientocclusion", "autoexposure",
             "present"},
}


def pass_group(label):
    name = Path(label).name.lower()
    for suffix in (".comp.spv", ".frag.spv", ".vert.spv"):
        name = name.removesuffix(suffix)
    for group, labels in GROUPS.items():
        if name in labels:
            return group
    if "shadow" in name:
        return "shadows"
    if name.startswith("cloud") or name.startswith("atmosphere"):
        return "atmosphere"
    return None


def distribution(values):
    if not values or any(v is None for v in values):
        return None
    ordered = sorted(values)
    return {"p50_ms": ordered[math.ceil(len(ordered) * .5) - 1],
            "p99_ms": ordered[math.ceil(len(ordered) * .99) - 1],
            "mean_ms": statistics.mean(ordered)}


def read_vulkan_profile(directory, report):
    paths = sorted(directory.glob("gpu-profile.*.json"))
    result = {"paths": [str(p) for p in paths], "frames": [], "gpu_passes": None,
              "draws_max": None, "triangles_max": None,
              "time_basis": "inclusive native pass intervals; overlapping passes are not additive",
              "count_scope": "all raster passes, including terrain, shadows and postprocessing"}
    samples = sorted(report["gpu_times"]["samples"], key=lambda row: row["submission"])
    if not paths or len(samples) != report["motion"]["frames"]:
        result["error"] = "native pass profile or measured GPU submission identities unavailable"
        return result
    submissions = {}
    for path in paths:
        profile = json.loads(path.read_text())
        if profile["schema"] != 1 or profile["backend"] != "Vulkan":
            raise ValueError("unsupported native Vulkan profile")
        for submission in profile["submissions"]:
            key = profile["device"], submission["serial"]
            if key in submissions:
                raise ValueError("duplicate native profile submission")
            submissions[key] = submission
    measured = [(row["device"], row["submission"]) for row in samples]
    if len(set(measured)) != len(measured) or any(k not in submissions for k in measured):
        result["error"] = "native profile does not cover every measured GPU submission"
        return result
    for frame, key in enumerate(measured, 1):
        submission = submissions[key]
        passes = submission["passes"]
        valid = submission["dropped_passes"] == 0
        times = {group: [] for group in GROUPS}
        unknown = []
        draws, triangles = 0, 0
        for item in passes:
            group = pass_group(item["label"])
            ns = item["gpu_ns"]
            available = item["available"] and ns is not None and math.isfinite(ns) and ns >= 0
            if group:
                times[group].append(ns / 1e6 if available else None)
            else:
                unknown.append(item["label"])
            if item["kind"] == "render":
                count, inputs = item["draw_calls"], item["input_primitives"]
                if not isinstance(count, int) or count < 0:
                    raise ValueError("invalid native draw count")
                draws += count
                if inputs is not None and (not isinstance(inputs, int) or inputs < 0):
                    raise ValueError("invalid native input primitive count")
                triangles = triangles + inputs if triangles is not None and inputs is not None else None
        status = {group: "truncated" if not valid else "unclassified" if unknown else
                  "not_executed" if not values else "unavailable" if None in values else "measured"
                  for group, values in times.items()}
        grouped = {group: sum(values) if status[group] == "measured" else None
                   for group, values in times.items()}
        result["frames"].append({"frame": frame, "device": key[0], "submission": key[1],
                                  "gpu_passes": grouped, "gpu_pass_status": status,
                                  "unclassified": unknown,
                                  "draw_calls": draws if valid else None,
                                  "input_triangles": triangles if valid else None})
    result["gpu_passes"] = {}
    for group in GROUPS:
        frames = result["frames"]
        known = all(f["gpu_pass_status"][group] in ("measured", "not_executed") for f in frames)
        values = [f["gpu_passes"][group] for f in frames if f["gpu_pass_status"][group] == "measured"]
        measured = distribution(values) if known else None
        if measured is not None:
            measured.update({"executed_frames": len(values), "total_frames": len(frames)})
        result["gpu_passes"][group] = measured
    for field, output in (("draw_calls", "draws_max"), ("input_triangles", "triangles_max")):
        values = [f[field] for f in result["frames"]]
        if all(v is not None for v in values):
            result[output] = max(values)
    return result
