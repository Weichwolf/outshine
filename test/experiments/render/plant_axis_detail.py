"""Select capped circular-axis detail before meshing; test its parametric error model."""

import argparse
import json
import math
import random
import time
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import PlantPlan

MIN_SEGMENTS, MAX_SEGMENTS = 2, 1024
MIN_SIDES, MAX_SIDES = 3, 128


def coefficients(length, bend, base, tip):
    bend = abs(bend)
    radius = max(base, tip)
    slope = abs(tip - base)
    return (
        (length * bend + 2 * slope * bend + radius * bend * bend) / 8,
        (slope + radius * bend) * math.pi / 2,
        radius * math.pi * math.pi / 2,
    )


def error(coeff, segments, sides):
    a, b, c = coeff
    return a / (segments * segments) + b / (segments * sides) + c / (sides * sides)


def select(coeff, allowed):
    best = None
    evaluations = 0
    for sides in range(MIN_SIDES, MAX_SIDES + 1):
        if best is not None and 2 * sides * (MIN_SEGMENTS + 1) >= best[0]:
            break
        evaluations += 1
        if error(coeff, MAX_SEGMENTS, sides) > allowed:
            continue
        lo, hi = MIN_SEGMENTS, MAX_SEGMENTS
        while lo < hi:
            middle = (lo + hi) // 2
            evaluations += 1
            if error(coeff, middle, sides) <= allowed:
                hi = middle
            else:
                lo = middle + 1
        triangles = 2 * sides * (lo + 1)
        if best is None or triangles < best[0]:
            best = triangles, lo, sides, error(coeff, lo, sides)
    return best, evaluations


def exhaustive(coeff, allowed):
    segments = np.arange(MIN_SEGMENTS, MAX_SEGMENTS + 1)[None, :]
    sides = np.arange(MIN_SIDES, MAX_SIDES + 1)[:, None]
    a, b, c = coeff
    errors = a / segments**2 + b / (segments * sides) + c / sides**2
    triangles = np.broadcast_to(2 * sides * (segments + 1), errors.shape).copy()
    triangles[errors > allowed] = np.iinfo(np.int64).max
    if np.all(triangles == np.iinfo(np.int64).max):
        return None
    y, x = np.unravel_index(np.argmin(triangles), triangles.shape)
    return int(triangles[y, x]), int(segments[0, x]), int(sides[y, 0])


def surface(profile, t, phi):
    length, bend, base, tip = profile
    angle = bend * t
    radius = base + (tip - base) * t
    centre_y = length * t * np.sinc(angle / np.pi)
    centre_z = length * bend * t * t / 2 * np.sinc(angle / (2 * np.pi)) ** 2
    return np.stack(
        (
            radius * np.cos(phi),
            centre_y - radius * np.sin(phi) * np.sin(angle),
            centre_z + radius * np.sin(phi) * np.cos(angle),
        ),
        axis=-1,
    )


def sample_error(profile, segments, sides, source):
    t, phi = source.random((2, 4096))
    row = np.floor(t * segments)
    side = np.floor(phi * sides)
    u, v = t * segments - row, phi * sides - side
    t0, t1 = row / segments, (row + 1) / segments
    p0, p1 = side * 2 * np.pi / sides, (side + 1) * 2 * np.pi / sides
    a, b, c, d = (
        surface(profile, *q) for q in ((t0, p0), (t0, p1), (t1, p1), (t1, p0))
    )
    lower = (1 - u)[:, None] * a + (u - v)[:, None] * d + v[:, None] * c
    upper = (1 - v)[:, None] * a + u[:, None] * c + (v - u)[:, None] * b
    interpolated = np.where((u >= v)[:, None], lower, upper)
    return float(
        np.linalg.norm(
            surface(profile, t, phi * 2 * np.pi) - interpolated, axis=1
        ).max()
    )


def run(output, recipes):
    source = random.Random(718)
    profiles = [(1.0, 0.0, 0.1, 0.1), (1.0, 0.0, 0.1, 0.01), (1.0, -3.0, 0.2, 0.01)]
    for _ in range(97):
        length = 10 ** source.uniform(-2, 1)
        bend = source.uniform(-math.pi, math.pi)
        base = length / (4 + abs(bend)) * source.uniform(0.01, 0.99)
        profiles.append((length, bend, base, base * source.uniform(0.01, 1)))
    rng = np.random.default_rng(718)
    maximum_fraction, evaluations = 0.0, 0
    for profile in profiles:
        coeff = coefficients(*profile)
        allowed = profile[0] * 10 ** source.uniform(-4, -1)
        chosen, work = select(coeff, allowed)
        brute = exhaustive(coeff, allowed)
        if (chosen[:3] if chosen else None) != brute:
            raise ValueError((profile, allowed, chosen, brute))
        evaluations += work
        if chosen:
            measured = sample_error(profile, chosen[1], chosen[2], rng)
            if measured > chosen[3] * (1 + 1e-12):
                raise ValueError((profile, chosen, measured))
            maximum_fraction = max(maximum_fraction, measured / chosen[3])
    results = []
    for path in recipes:
        plan = PlantPlan.from_recipe(json.loads(path.read_text()))
        rows = []
        for path_in_tree in plan.paths():
            axis = plan.axis(path_in_tree)
            level = plan.levels[len(path_in_tree)]
            rows.append((axis.length, axis.bend, (level.base_radius, level.tip_radius)))
        for allowed in (0.005, 0.01):
            started = time.perf_counter()
            selected = [
                select(
                    coefficients(float(length), float(bend), *map(float, radii)),
                    allowed,
                )
                for length, bend, radii in rows
            ]
            if any(choice is None for choice, _ in selected):
                raise ValueError("recipe detail is outside bounded tessellation range")
            results.append(
                {
                    "recipe": path.name,
                    "axes": len(rows),
                    "allowed_parametric_error_m": allowed,
                    "wood_triangles": sum(choice[0] for choice, _ in selected),
                    "fixed8x6_wood_triangles": len(rows) * 108,
                    "evaluations": sum(work for _, work in selected),
                    "selection_ms": (time.perf_counter() - started) * 1000,
                }
            )
    receipt = {
        "profiles": len(profiles),
        "exhaustive_integer_choices_equal": True,
        "largest_sample_to_bound_ratio": maximum_fraction,
        "evaluations": evaluations,
        "recipes": results,
        "scope": "ideal parametric capped tube only; no Float32/shader, normal, radiance or runtime LOD bound",
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--recipe", type=Path, action="append", default=[])
    args = parser.parse_args()
    run(args.output, args.recipe)


if __name__ == "__main__":
    main()
