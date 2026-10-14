"""Compare equal-hash analytic rock gradients with screen differences at near-camera scale."""

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from rock_direction import BEDDING, FRACTURES, value

ROOT = Path(__file__).resolve().parents[3]


def noise_gradient(at):
    dtype = at.dtype
    cell = np.floor(at).astype(np.int64)
    f = at-cell.astype(dtype)
    smooth = f*f*(3.-2.*f)
    derivative = 6.*f*(1.-f)
    result = np.zeros(at.shape[:-1], dtype=dtype)
    gradient = np.zeros_like(at)
    for z in range(2):
        for y in range(2):
            for x in range(2):
                corner = np.array([x, y, z])
                weights = np.where(corner, smooth, 1.-smooth)
                tap = value(cell+corner).astype(dtype)
                result += tap*np.prod(weights, axis=-1)
                for axis in range(3):
                    other = np.prod(np.delete(weights, axis, axis=-1), axis=-1)
                    gradient[..., axis] += tap*other*derivative[..., axis]*(1 if corner[axis] else -1)
    return result, gradient


def rock_bump(world, recipe):
    coarse, fine = recipe['detailScaleM']
    scales = [coarse*12., coarse, fine]
    matrices = [np.eye(3), BEDDING, FRACTURES]
    amplitudes = [min(scales[0]*.025, 3.), min(coarse*.04, .4), recipe['heightAmplitudeM']]
    bump = np.zeros(world.shape[:-1], dtype=world.dtype)
    gradient = np.zeros_like(world)
    for scale, matrix, amplitude in zip(scales, matrices, amplitudes):
        transform = matrix.astype(world.dtype)/scale
        n, g = noise_gradient(world@transform.T)
        bump += amplitude*(n-.5)
        gradient += amplitude*(g@transform)
    return bump, gradient


def surface_gradient(n, dx, dy, hx, hy, normalized):
    if normalized:
        scale = max(np.max(np.abs(dx)), np.max(np.abs(dy)))
        if scale == 0: return np.zeros(3)
        dx, dy, hx, hy = dx/scale, dy/scale, hx/scale, hy/scale
    a, b = np.cross(dy, n), np.cross(n, dx)
    determinant = np.dot(dx, a)
    return np.zeros(3) if abs(determinant) < 1e-8 else (a*hx+b*hy)/determinant


def render(normals):
    light = np.array([.4, .7, .5]); light /= np.linalg.norm(light)
    brightness = .2+.8*np.clip(np.sum(normals*light, axis=-1), 0., 1.)
    return Image.fromarray(np.uint8(np.repeat(brightness[..., None], 3, axis=-1)*255))


def main():
    output = ROOT/'build/experiments/render/ground-gradients'; output.mkdir(parents=True, exist_ok=True)
    recipe = next(r for r in json.loads((ROOT/'src/assets/world/ground-materials.json').read_text())['classes'] if r['name']=='limestone')
    points = np.random.default_rng(2169).uniform(-100., 100., (512, 3))
    _, exact = rock_bump(points, recipe)
    estimated = np.empty_like(exact)
    for axis in range(3):
        delta = np.zeros(3); delta[axis] = 1e-5
        estimated[:, axis] = (rock_bump(points+delta, recipe)[0]-rock_bump(points-delta, recipe)[0])/2e-5
    derivative_error = float(np.max(np.abs(exact-estimated)))
    assert derivative_error < 1e-7
    spacing = .0001
    x, north = np.meshgrid(np.arange(320)*spacing, np.arange(240)*spacing)
    world = np.stack([x, 1770.773+.54*x+.85*north, north], axis=-1)
    normals = np.array([-.54, 1., -.85]); normals /= np.linalg.norm(normals)
    bump, gradient = rock_bump(world.astype(np.float32), recipe)
    reference = rock_bump(world, recipe)[1]
    derivatives = np.gradient(bump, spacing)
    finite_tangent = np.stack([derivatives[1], np.zeros_like(x), derivatives[0]], axis=-1)
    screen_reference = np.stack([reference[..., 0]+.54*reference[..., 1], np.zeros_like(x), reference[..., 2]+.85*reference[..., 1]], axis=-1)
    basis = np.array([[1., 0.], [.54, .85], [0., 1.]])
    dual = basis@np.linalg.inv(basis.T@basis)
    finite_gradient = np.stack([derivatives[1], derivatives[0]], axis=-1)@dual.T
    directions = []
    for g in [finite_gradient, gradient, reference]:
        tangent = g-normals*np.sum(g*normals, axis=-1)[..., None]
        n = normals-tangent; n /= np.linalg.norm(n, axis=-1, keepdims=True)
        directions.append(n)
    canvas = Image.new('RGB', (960, 270), (24,24,24)); draw = ImageDraw.Draw(canvas)
    for column, (label, n) in enumerate(zip(['Float32 screen differences', 'Float32 analytic gradient', 'Float64 analytic reference'], directions)):
        canvas.paste(render(n), (column*320,30)); draw.text((column*320+5,8), label, fill='white')
    canvas.save(output/'near-normal-comparison.png')
    scale_controls = []
    for scale in [1., .01, .001, .0001, .00001, .000001]:
        dx = np.array([scale, 0., 0.]); dy = np.array([0., 0., scale])
        fixed = surface_gradient(np.array([0., 1., 0.]), dx, dy, .1*scale, .2*scale, True)
        old = surface_gradient(np.array([0., 1., 0.]), dx, dy, .1*scale, .2*scale, False)
        np.testing.assert_allclose(fixed, [.1, 0., .2], rtol=0, atol=1e-15)
        scale_controls.append(dict(scale_m=scale, old_error=float(np.max(np.abs(old-[.1, 0., .2])))))
    report = dict(scale_controls=scale_controls, spacing_m=spacing, height_m=1770.773, independent_derivative_error=derivative_error,
                  screen_gradient_rmse=float(np.sqrt(np.mean((finite_tangent-screen_reference)**2))),
                  analytic_gradient_rmse=float(np.sqrt(np.mean((gradient-reference)**2))),
                  hashes_per_sample=24, scope='Fixed footprint/recipe, equal hashes; isolated noise-height precision. Not a complete GPU, blend, normal-filter or pixel-budget proof.')
    (output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps(report))


if __name__=='__main__':
    main()
