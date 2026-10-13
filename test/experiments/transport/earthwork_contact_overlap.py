import json
import math

from earthwork_transition import smoothstep


def height(north_m, source_m, bed_m, blended):
    distances = (max(0, abs(north_m) - 2), math.hypot(20, max(0, abs(north_m) - 2)))
    beds = (bed_m, -bed_m)
    widths = [math.hypot(30, 1.875 * abs(bed - source_m) * 1.5) for bed in beds]
    fades = [smoothstep(min(1, distance / width)) for distance, width in zip(distances, widths)]
    if not blended:
        offers = [source_m + (bed - source_m) * (1 - fade) for bed, fade in zip(beds, fades)]
        if min(offers) < source_m:
            return min(offers)
        contacts = [bed for bed, distance in zip(beds, distances) if distance == 0]
        return min(max(offers), min(contacts, default=math.inf))
    if distances[0] == 0:
        return bed_m
    weights = [(1 - fade) / fade for fade in fades]
    return source_m + sum(weight * (bed - source_m) for weight, bed in zip(weights, beds)) / (1 + sum(weights))


def compare(source_m, bed_m):
    step_m = .001
    samples = [2 + sample * step_m for sample in range(-10, 11)]
    old = [height(n, source_m, bed_m, False) for n in samples]
    new = [height(n, source_m, bed_m, True) for n in samples]
    return dict(source_m=source_m, contact_m=bed_m,
                old_core_error_m=abs(old[0] - bed_m),
                new_core_error_m=abs(new[0] - bed_m),
                new_max_boundary_slope=max(abs(b - a) / step_m for a, b in zip(new, new[1:])),
                assumption='two connected flat contacts; 20 m separation; flat source terrain')


if __name__ == '__main__':
    print(json.dumps([compare(source, bed) for source in (-10, 0, 10) for bed in (-6, 6)], indent=2))
