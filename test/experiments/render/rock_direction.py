"""Compare equal-sample rock fields on a metric cliff; not a runtime/GPU proof."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
BEDDING = np.array([[.12, 1., .08], [.125, 0., 0.], [0., 0., .125]])
FRACTURES = np.array([[.94, .04, .34], [0., 1./6., 0.], [-.34/6., 0., .94/6.]])


def value(cell):
    p = cell.astype(np.uint32)
    h = p[..., 0]*np.uint32(0x9e3779b9) ^ p[..., 1]*np.uint32(0x85ebca6b) ^ p[..., 2]*np.uint32(0xc2b2ae35)
    h ^= h >> 16
    h *= np.uint32(0x7feb352d)
    h ^= h >> 15
    h *= np.uint32(0x846ca68b)
    h ^= h >> 16
    return (h & np.uint32(0xffffff)).astype(float)/16777215.


def noise(at):
    cell = np.floor(at).astype(np.int64)
    f = at-cell
    f = f*f*(3.-2.*f)
    result = np.zeros(at.shape[:-1])
    for z in range(2):
        for y in range(2):
            for x in range(2):
                weight = np.prod(np.where(np.array([x,y,z]), f, 1.-f), axis=-1)
                result += weight*value(cell+np.array([x,y,z]))
    return result


def filtered(at, footprint, scale):
    t = np.clip((footprint/scale-.25)/.55, 0., 1.)
    visibility = 1.-t*t*(3.-2.*t)
    return (noise(at/scale)-.5)*visibility if visibility > 0 else np.zeros(at.shape[:-1])


def field(world, footprint, recipe, directed):
    coarse, fine = recipe['detailScaleM']
    macro = filtered(world, footprint, coarse*12.)
    bedding = filtered(world@BEDDING.T if directed else world, footprint*1.04 if directed else footprint, coarse)
    fractures = filtered(world@FRACTURES.T if directed else world, footprint*1.04 if directed else footprint, fine)
    bump = min(coarse*12.*.025, 3.)*macro+min(coarse*.04,.4)*bedding+recipe['heightAmplitudeM']*fractures
    colour = 1.+.6*(.4*macro+.5*bedding+.1*fractures) if directed else 1.+.6*(.65*macro+.25*bedding+.1*fractures)
    return colour, bump, bedding


def picture(world, spacing, recipe, directed):
    colour, bump, bedding = field(world, spacing, recipe, directed)
    dy, dx = np.gradient(bump, spacing)
    normal = np.stack([-dx,-dy,np.ones_like(dx)], axis=-1)
    normal /= np.linalg.norm(normal,axis=-1,keepdims=True)
    light = np.array([.4,.6,.6]);light /= np.linalg.norm(light)
    assert np.isfinite(normal).all()
    lambert = .25+.75*np.clip(np.sum(normal*light,axis=-1),0.,1.)
    linear = np.array(recipe['albedo'])*colour[...,None]*lambert[...,None]
    pixels = np.clip(np.where(linear<=.0031308,linear*12.92,1.055*linear**(1./2.4)-.055),0.,1.)
    return Image.fromarray(np.uint8(pixels*255)), bedding


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/experiments/render/rock-direction')
    args = parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    source = ROOT/'src/assets/world/ground-materials.json'
    recipe = next(r for r in json.loads(source.read_text())['classes'] if r['name']=='rock')
    spacing = .125
    x,y = np.meshgrid(np.arange(640)*spacing,np.arange(480)*spacing)
    world = np.stack([x+1000.,y+200.,np.full_like(x,350.)],axis=-1)
    canvas = Image.new('RGB',(1280,510),(24,24,24));draw=ImageDraw.Draw(canvas)
    results=[]
    for at,directed in enumerate((False,True)):
        began=time.perf_counter();image,bedding=picture(world,spacing,recipe,directed)
        elapsed=time.perf_counter()-began
        canvas.paste(image,(at*640,30));draw.text((at*640+10,10),'Directed bedding/fractures' if directed else 'Existing isotropic noise',fill='white')
        dy,dx=np.gradient(bedding,spacing)
        results.append(dict(directed=directed,python_seconds=elapsed,
                            bedding_gradient_ratio_y_over_x=float(np.mean(dy*dy)/np.mean(dx*dx))))
    canvas.save(args.output/'cliff-comparison.png')
    bounds=[float(np.linalg.svd(matrix,compute_uv=False)[0]) for matrix in (BEDDING,FRACTURES)]
    assert all(bound<1.04 for bound in bounds)
    _,bump,_=field(world,1e6,recipe,True)
    assert np.array_equal(bump,np.zeros_like(bump))
    report=dict(results=results,transform_operator_norms=bounds,filter_bound=1.04,
                noise_corner_hashes_per_fully_resolved_sample=24,
                scope='Fixed native world frame, equal three lattice fields; bounded frequency fade, not exact band-limiting or Gabor convolution. Python timings do not predict GPU cost.',
                material_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                experiment_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    (args.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__=='__main__':
    main()
