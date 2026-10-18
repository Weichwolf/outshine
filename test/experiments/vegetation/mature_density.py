"""Bounded branch-degree search for explicit nominal needle-area targets."""

import hashlib
import json
from dataclasses import replace

import numpy as np
from conifer_whole_products import BASE

WINDOW_SETTINGS = {"nominal_span_m": 0.32, "max_groups": 256}
MAX_AXES = 65536
MAX_WINDOWS = 100000
MAX_RELATIVE_AREA_ERROR = 0.05


def needle_areas(species, source=BASE):
    directory = source / species
    path = directory / "shoot.json"
    scene = json.loads(path.read_text())
    inputs = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()}
    areas = []
    for product in scene["products"]:
        if product["surface"] != "leaf":
            continue
        path = directory / product["mesh"]
        inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        with np.load(path, allow_pickle=False) as stored:
            points = stored["vertices"][stored["faces"]]
        cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        areas.append(float(np.linalg.norm(cross, axis=1).sum() / 4))
    return tuple(areas), inputs


def match_area(plan, unit_areas, target_area_m2):
    if not np.isfinite(target_area_m2) or target_area_m2 <= 0:
        raise ValueError("finite positive nominal foliage-area target required")
    candidates = []
    for degree in range(1, 65):
        last = replace(plan.wood.levels[-1], branches=degree)
        wood = replace(plan.wood, levels=(*plan.wood.levels[:-1], last))
        candidate = replace(plan, wood=wood)
        summary = candidate.window_summary(**WINDOW_SETTINGS)
        if summary["axes"] > MAX_AXES or summary["windows"] > MAX_WINDOWS:
            continue
        area = candidate.area_summary(unit_areas)["expected_area_m2"]
        candidates.append(
            (abs(area - target_area_m2), summary["axes"], degree, area, candidate)
        )
    if not candidates:
        raise ValueError("no complete crown recipe fits the declared density budgets")
    best = min(candidates, key=lambda row: row[:3])
    if best[0] / target_area_m2 > MAX_RELATIVE_AREA_ERROR:
        raise ValueError(
            "complete crown cannot meet its area target in the declared recipe family"
        )
    return best[-1], {
        "target_area_m2": float(target_area_m2),
        "expected_area_m2": best[3],
        "relative_area_error": best[0] / target_area_m2,
        "terminal_branches": best[2],
        "feasible_candidates": len(candidates),
        "selection": "exhaustive minimum area error, then axis count, then degree in fixed family 1..64; not a global plant optimum",
        "area_convention": "half cached closed needle-prototype area; uniform scalar variation expectation; declared circular crown footprint, not measured stand LAI",
    }
