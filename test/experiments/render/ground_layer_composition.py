"""Choose a cheap ground/litter roughness proxy against the renderer's dielectric GGX."""

import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]


def light_samples():
    angles = np.deg2rad(np.array([0., 15., 30., 45., 60., 75.]))
    azimuths = np.deg2rad(np.arange(0., 360., 30.))
    v, l, phi = np.meshgrid(angles, angles, azimuths, indexing='ij')
    nv, nl = np.cos(v), np.cos(l)
    vl = np.cos(v)*np.cos(l) + np.sin(v)*np.sin(l)*np.cos(phi)
    length = np.sqrt(2.+2.*vl)
    return nl.ravel(), nv.ravel(), ((nl+nv)/length).ravel(), (length*.5).ravel()


def reflection(colour, roughness, samples):
    nl, nv, nh, vh = samples
    a2 = roughness**4
    distribution = a2 / (np.pi*(nh*nh*(a2-1.)+1.)**2)
    visibility = .5 / (nl*np.sqrt(nv*nv*(1.-a2)+a2) + nv*np.sqrt(nl*nl*(1.-a2)+a2))
    fresnel = .04+.96*(1.-vh)**5
    diffuse = (1.-fresnel)[:, None]*np.asarray(colour)[None, :]/np.pi
    specular = (fresnel*distribution*visibility)[:, None]
    return (diffuse+specular)*nl[:, None]


def main():
    paths = [ROOT / 'src/assets/world/ground-materials.json',
             ROOT / 'src/assets/world/vegetation.json']
    materials, recipes = [json.loads(path.read_text()) for path in paths]
    classes = {row['name']: row for row in materials['classes']}
    samples = light_samples()
    results = []
    for recipe in recipes['templates']:
        ground = recipe['ground']
        base = classes[ground['class']]
        litter_name = ground.get('litterClass') or base['litter']['class']
        if not litter_name:
            continue
        litter = classes[litter_name]
        coverage = ground.get('litterCoverage', base['litter']['coverage'])
        if not 0. <= coverage <= 1.:
            raise ValueError('invalid declared coverage')
        colour = np.asarray(base['albedo'])*(1.-coverage) + np.asarray(litter['albedo'])*coverage
        target = (reflection(base['albedo'], base['roughness'], samples)*(1.-coverage)
                  + reflection(litter['albedo'], litter['roughness'], samples)*coverage)
        candidates = {'perceptual': base['roughness']*(1.-coverage)+litter['roughness']*coverage,
                      'alpha': np.sqrt(base['roughness']**2*(1.-coverage)+litter['roughness']**2*coverage),
                      'alpha_squared': (base['roughness']**4*(1.-coverage)+litter['roughness']**4*coverage)**.25}
        errors = {}
        for name, roughness in candidates.items():
            actual = reflection(colour, roughness, samples)
            relative = np.abs(actual-target)/np.maximum(target, .01)
            errors[name] = dict(roughness=float(roughness), rms=float(np.sqrt(np.mean(relative**2))),
                                maximum=float(relative.max()))
        results.append(dict(template=recipe['name'], coverage=coverage,
                            nominal_colour=colour.tolist(), candidates=errors))
    totals = {name: float(np.mean([row['candidates'][name]['rms'] for row in results]))
              for name in ('perceptual', 'alpha', 'alpha_squared')}
    output = ROOT / 'build/experiments/render/ground-layer-composition'
    output.mkdir(parents=True, exist_ok=True)
    report = dict(scope='432 direct-light direction pairs; dielectric f0=.04; nominal dry config colours; no IBL or runtime proof',
                  experiment_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  renderer_brdf_sha256=hashlib.sha256((ROOT / 'src/render/shaders/metalRoughBrdf.glsl').read_bytes()).hexdigest(),
                  inputs={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},
                  direction_pairs=len(samples[0]), relative_error_floor=.01,
                  mean_rms=totals, selected=min(totals, key=totals.get), results=results)
    (output / 'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(templates=len(results), mean_rms=totals, selected=report['selected'])))


if __name__ == '__main__':
    main()
