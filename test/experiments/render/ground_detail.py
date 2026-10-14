"""Compare metric substrate detail with its uniform far-field mean."""

import argparse
import hashlib
import json
from pathlib import Path
import time
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image, ImageDraw

from rock_direction import noise, value

ROOT = Path(__file__).resolve().parents[3]


def noise2(at):
    cell = np.floor(at).astype(np.int64)
    frac = at-cell
    frac = frac*frac*(3.-2.*frac)
    result = np.zeros(at.shape[:-1])
    for y in range(2):
        for x in range(2):
            shift = np.array([x, y])
            key = np.concatenate([cell+shift, np.zeros(cell.shape[:-1]+(1,), dtype=np.int64)], axis=-1)
            result += np.prod(np.where(shift, frac, 1.-frac), axis=-1)*value(key)
    return result


def filtered(at, footprint, scale):
    t = np.clip((footprint/scale-.25)/.55, 0., 1.)
    visibility = 1.-t*t*(3.-2.*t)
    return (noise2(at/scale)-.5)*visibility if visibility > 0 else np.zeros(at.shape[:-1])


def detail(at, footprint, recipe):
    coarse, fine = recipe['detailScaleM']
    c = filtered(at, footprint, coarse)
    f = filtered(at, footprint, fine)
    return 1.+.5*(.7*c+.3*f), .06*c+.04*f, recipe['heightAmplitudeM']*f


def grains(at, footprint, recipe, radius=1):
    grain = recipe['grainSizeM']
    if grain <= 0 or footprint >= grain*.8:
        return np.ones(at.shape[:-1]), np.zeros(at.shape[:-1])
    t = np.clip((footprint/grain-.25)/.55, 0., 1.)
    visibility = 1.-t*t*(3.-2.*t)
    point = at/grain
    cell = np.floor(point).astype(np.int64)
    nearest = np.full(at.shape[:-1], np.inf)
    tint = np.zeros(at.shape[:-1])
    for y in range(-radius, radius+1):
        for x in range(-radius, radius+1):
            site = cell+np.array([x, y])
            key = np.concatenate([site, np.zeros(site.shape[:-1]+(1,), dtype=np.int64)], axis=-1)
            bits = np.rint(value(key)*16777215.).astype(np.uint32)
            jitter = .25+.5*np.stack([bits & 255, (bits >> 8) & 255], axis=-1)/255.
            distance = np.sum((site+jitter-point)**2, axis=-1)
            nearer = distance < nearest
            tint = np.where(nearer, ((bits >> 16) & 255)/255.-.5, tint)
            nearest = np.minimum(nearest, distance)
    height = np.clip(1.-np.sqrt(nearest), 0., 1.)**2
    return 1.+.35*visibility*tint, recipe['heightAmplitudeM']*visibility*height


def picture(at, spacing, recipe, varied):
    if varied:
        scale, rough, bump = detail(at, spacing, recipe)
        tint, grain_bump = grains(at, spacing, recipe)
        scale *= tint
        bump += grain_bump
    else:
        scale = np.ones(at.shape[:-1])
        bump = np.zeros(at.shape[:-1])
        rough = np.zeros(at.shape[:-1])
    dy, dx = np.gradient(bump, spacing)
    normal = np.stack([-dx, -dy, np.ones_like(dx)], axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    light = np.array([.4, .6, .6]); light /= np.linalg.norm(light)
    lambert = .25+.75*np.clip(np.sum(normal*light, axis=-1), 0., 1.)
    linear = np.array(recipe['albedo'])*scale[..., None]*lambert[..., None]
    rgb = np.where(linear <= .0031308, linear*12.92, 1.055*linear**(1./2.4)-.055)
    return Image.fromarray(np.uint8(np.clip(rgb, 0., 1.)*255)), scale, rough


def write_controls(output):
    cases = [('GroundDetail', 1.6, 240., 60.), ('GroundGrains', .2, 240., 60.),
             ('GroundGrainsZoom', 1.6, 210., 6.)]
    for name, height, bearing, fov in cases:
        tree = ET.parse(ROOT/'src/assets/places/Koerbersee.scenario')
        root = tree.getroot()
        root.find('world').set('vegetation', 'no')
        root.find('views/view').set('fovDeg', str(fov))
        at = root.find('views/view/at')
        at.set('heightM', str(height)); at.set('samplesHeight', 'yes')
        at.set('bearingDeg', str(bearing)); at.set('pitchDeg', '-55')
        tree.write(output/(name+'.scenario'), encoding='unicode')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/experiments/render/ground-detail')
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    write_controls(args.output)
    source = ROOT/'src/assets/world/ground-materials.json'
    recipes = json.loads(source.read_text())['classes']
    spacing = .02
    x, y = np.meshgrid(np.arange(320)*spacing+1000., np.arange(240)*spacing+200.)
    at = np.stack([x, y], axis=-1)
    checks = np.stack([at[..., 0], at[..., 1], np.zeros_like(x)], axis=-1)
    error = float(np.max(np.abs(noise2(at)-noise(checks))))
    assert error < 1e-14
    gravel = next(r for r in recipes if r['name'] == 'gravel')
    probes = np.random.default_rng(2169).uniform(-100., 100., size=(1024, 2))
    small = grains(probes, .002, gravel)
    large = grains(probes, .002, gravel, radius=2)
    assert all(np.array_equal(a, b) for a, b in zip(small, large))
    assert np.sqrt(2.)*.75 < 1.25
    grain_far = grains(probes, 1e6, gravel)
    assert np.array_equal(grain_far[0], np.ones(len(probes)))
    assert np.array_equal(grain_far[1], np.zeros(len(probes)))
    names = ['earth_dry', 'gravel', 'forest_floor']
    canvas = Image.new('RGB', (1280, 810), (24, 24, 24)); draw = ImageDraw.Draw(canvas)
    results = []
    for row, name in enumerate(names):
        recipe = next(r for r in recipes if r['name'] == name)
        for column, varied in enumerate((False, True, False, True)):
            sample_spacing = spacing if column < 2 else .002
            sample_at = at if column < 2 else (at-np.array([1000., 200.]))*.1+np.array([1000., 200.])
            began = time.perf_counter()
            image, scale, rough = picture(sample_at, sample_spacing, recipe, varied)
            elapsed = time.perf_counter()-began
            canvas.paste(image, (column*320, row*270+30))
            draw.text((column*320+8, row*270+8), name+(' / detail' if varied else ' / uniform')+f' / {sample_spacing*320:g} m', fill='white')
            assert np.isfinite(scale).all() and np.isfinite(rough).all()
            assert np.min(scale) >= .6 and np.max(scale) <= 1.5
            far = detail(at, 1e6, recipe)
            assert np.array_equal(far[0], np.ones_like(x))
            assert np.array_equal(far[1], np.zeros_like(x)) and np.array_equal(far[2], np.zeros_like(x))
            results.append(dict(material=name, varied=varied, sample_spacing_m=sample_spacing, seconds=elapsed,
                                mean_albedo_scale=float(np.mean(scale)), contrast=float(np.std(scale))))
    canvas.save(args.output/'substrates.png')
    report = dict(results=results, max_equal_plane_error=error, corner_hashes_per_resolved_sample=8, gravel_extra_hashes=9, gravel_wide_reference_hashes=25, grain_reference_samples=1024,
                  same_cell_max_distance_cells=float(np.sqrt(2.)*.75), outside_stencil_min_distance_cells=1.25,
                  scope='Metric two-field preview and bounded 3x3-cell gravel relief. Nine extra hashes only for resolved substrate grains; no leaf-litter shapes. Footprint fade is an approximation, not exact band limiting. No runtime/GPU/BRDF or complete litter-recipe proof.',
                  material_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  experiment_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    (args.output/'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'results'}))


if __name__ == '__main__':
    main()
