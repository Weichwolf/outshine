import argparse
import json
import math


def smoothstep(t):
    return t * t * t * (10 + t * (-15 + 6 * t))


def compare(delta, minimum_width, slope):
    width = math.hypot(minimum_width, 1.875 * delta / slope)
    step = width / 10000
    samples = [delta * (1 - smoothstep(index / 10000)) for index in range(10001)]
    old_edge = max(0, abs(delta) - minimum_width * slope)
    return {
        "height_difference_m": delta,
        "old_boundary_jump_m": old_edge,
        "smooth_transition_width_m": width,
        "new_boundary_jump_m": abs(samples[-1]),
        "measured_maximum_slope": max(abs(b - a) / step for a, b in zip(samples, samples[1:])),
        "slope_limit": slope,
        "assumption": "flat source terrain and constant contact height",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--height-differences", type=float, nargs="+", default=[.5, 6, 20])
    parser.add_argument("--minimum-width", type=float, default=6)
    parser.add_argument("--slope", type=float, default=2 / 3)
    args = parser.parse_args()
    values = [*args.height_differences, args.minimum_width, args.slope]
    if not all(math.isfinite(value) for value in values) or args.minimum_width <= 0 or args.slope <= 0:
        parser.error("finite heights and positive width/slope required")
    print(json.dumps([compare(delta, args.minimum_width, args.slope) for delta in args.height_differences], indent=2))
