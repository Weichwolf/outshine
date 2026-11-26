"""Bake the source ground palette into the regular grid of prepared surface tiles."""

import argparse
from pathlib import Path
import subprocess
import tempfile

import numpy as np

from pack_surface_tile import with_ground_colours
from terrain_cached_page import package, page_prefix


SAMPLE = np.dtype([("class", "<i4"), ("other", "<i4"), ("edge", "<f4"), ("slope", "<f4")])


def smooth(unit):
    t = np.clip(unit, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def ground_colour(palette, samples):
    if len(palette) < 4:
        raise ValueError("missing source ground palette")
    rows, rock, _, layers = map(int, palette.view("<u4")[:4])
    slopes_at = 4 * (rows + 2)
    surfaces_at = slopes_at + rows + 1
    if (not 0 <= rock < rows or layers != surfaces_at + 8 * (rows + 1)
            or len(palette) != layers + 8 * (rows + 1)):
        raise ValueError("unsupported source ground palette layout")
    colours = palette[4:slopes_at].reshape(-1, 4)[:, :3]
    limits = palette[slopes_at:surfaces_at]
    state = palette[surfaces_at:layers].reshape(-1, 8)
    if not np.isfinite(palette[4:]).all() or not np.isfinite(palette[2]) or palette[2] < 0:
        raise ValueError("nonfinite or invalid ground palette")

    def tint(row):
        slope_delta = samples["slope"] - limits[row]
        if palette[2] > 0:
            weight = smooth(slope_delta / palette[2])
        else:
            weight = (slope_delta > 0).astype(float)
        weight[row == rock] = 1
        return colours[row] * (1 - weight[:, None]) + colours[rock] * weight[:, None]

    first, second = (np.where((samples[field] >= 0) & (samples[field] < rows),
                             samples[field], rows) for field in ("class", "other"))
    reach = np.maximum(state[first, 5], state[second, 5])
    reach[(state[first, 6] > 0) | (state[second, 6] > 0)] = 0
    edge = samples["edge"]
    ratio = np.divide(edge, reach, out=np.zeros_like(edge), where=reach > 0)
    weight = np.where(reach > 0, smooth(.5 + .5 * ratio), .5 + .5 * np.sign(edge))
    rgb = tint(second) * (1 - weight[:, None]) + tint(first) * weight[:, None]
    result = np.full((len(rgb), 4), 255, dtype=np.uint8)
    result[:, :3] = np.rint(255 * np.clip(rgb, 0, 1)).astype(np.uint8)
    return result.tobytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path)
    parser.add_argument("region_key")
    parser.add_argument("sampler", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("tiles", type=Path, nargs="+")
    args = parser.parse_args()
    stored, _ = package(args.cache, args.region_key)
    region, _, _ = page_prefix(stored)
    args.output.mkdir()
    with tempfile.TemporaryDirectory(prefix="outshine-ground-colour-") as folder:
        work = Path(folder)
        source = work / "ground.region"
        source.write_bytes(region.data)
        subprocess.run([args.sampler, source, work, *args.tiles], check=True)
        palette = np.fromfile(work / "palette.f32", dtype="<f4")
        for tile in args.tiles:
            samples = np.fromfile(work / (tile.name + ".ground"), dtype=SAMPLE)
            data = with_ground_colours(tile.read_bytes(), ground_colour(palette, samples))
            with (args.output / tile.name).open("xb") as output:
                output.write(data)


if __name__ == "__main__":
    main()
