"""Compare clearance aprons; native Feldkirch inputs, no GPU or global slope proof."""
import json
import math


def distance(point, ring):
    def segment(a, b):
        edge = (b[0] - a[0], b[1] - a[1])
        t = max(0., min(1., sum((point[k] - a[k]) * edge[k] for k in (0, 1)) /
                       sum(value * value for value in edge)))
        return math.hypot(*(point[k] - a[k] - t * edge[k] for k in (0, 1)))
    return min(segment(a, b) for a, b in zip(ring, ring[1:] + ring[:1]))


def smooth(t):
    return t * t * t * (10 + t * (-15 + 6 * t))


def cut(source, bed, outside, width):
    if outside >= width:
        return source
    return source + min(0., max(-30., bed - source)) * (1 - smooth(outside / width))


def main():
    ring = [(224.78578787163366, -131.43658989430924),
            (226.0383756043811, -132.5164693923886),
            (228.97670144119542, -129.10820865167213),
            (227.724113708448, -128.02832915359278)]
    samples = [(212.8668871132731, -147.56354761959383, 508.22500058482046),
               (212.8669306319932, -148.37357034148815, 508.65570686511353)]
    apron, relief = 20.286770801477473, 13.524513867651649
    width = math.hypot(apron, 1.875 * relief * 1.5)
    legacy, faded = [], []
    for east, north, source in samples:
        outside = distance((east, north), ring)
        bed = (472.1149961426415 + .02048532191448895 * (east - 226.25495079004082) -
               .017660782210033702 * (north + 129.732459523951))
        legacy.append(min(source, bed + outside * (2 / 3)) if outside <= apron else source)
        faded.append(cut(source, bed, outside, width))
    assert max(abs(a - b) for a, b in zip(legacy, (485.4869802990325, samples[1][2]))) < 1.e-7
    flat_width = math.hypot(6., 1.875 * 20 * 1.5)
    heights = [cut(20., 0., i / 1000, flat_width) for i in range(60001)]
    peak_slope = max(abs(a - b) * 1000 for a, b in zip(heights, heights[1:]))
    assert peak_slope <= 2 / 3 and heights[0] == 0 and heights[-1] == 20
    print(json.dumps(dict(native_legacy_jump_m=abs(legacy[1] - legacy[0]),
                          proposed_jump_m=abs(faded[1] - faded[0]),
                          declared_clearance_width_m=apron, proposed_width_m=width,
                          flat_source_peak_slope=peak_slope,
                          meaning="isolated native clearance; overlapping contacts need native tests")))


if __name__ == '__main__':
    main()
