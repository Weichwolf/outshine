"""Check the black-box PRT reference adapter against analytic volumes and real provider inputs."""

import json
from pathlib import Path

import shapely

from esri_oracle import CASES, generate
from evidence import revision
from samples import read_site


def check_extrusion(sample):
    _, _, result = generate(sample, 'extrusion_rule.rpk')
    low, high = result['bounds_prt_m']
    lower, upper = result['provider_interval_m']
    expected = sample['polygon'].area*(upper-lower)
    error = abs(result['surface_integral_m3']-expected)
    if error > max(1.e-5, expected*1.e-8) or abs(low[1]-lower) > 1.e-5 or abs(high[1]-upper) > 1.e-5:
        raise AssertionError(f'PRT extrusion disagrees with analytic area × height: {sample["id"]}')
    if result['cga_messages']:
        raise AssertionError(result['cga_messages'])
    return dict(id=sample['id'], expected_volume_m3=expected, absolute_volume_error_m3=error,
                interval_m=[low[1], high[1]], holes=result['source_holes'])


def main():
    rectangle = [(0, 0), (20, 0), (20, 20), (0, 20)]
    polygons = [shapely.Polygon(rectangle), shapely.Polygon(rectangle, [[(5, 5), (5, 15), (15, 15), (15, 5)]]),
                shapely.Polygon([(0, 0), (20, 0), (20, 8), (8, 8), (8, 20), (0, 20)])]
    controls = []
    for i, polygon in enumerate(polygons):
        for base in (-3., 5.):
            sample = dict(id=f'analytic-{i}-{base}', polygon=polygon, source_sha256='analytic',
                          properties=dict(height=base+18., min_height=base))
            controls.append(check_extrusion(sample))
    real, stable, sites = [], [], {}
    for case, site, identifier in CASES:
        if site not in sites:
            sites[site] = read_site(site)[0]
        sample = next(s for s in sites[site] if s['id'] == identifier)
        real.append(check_extrusion(sample))
        _, _, a = generate(sample)
        _, _, b = generate(sample)
        if a['geometry_sha256'] != b['geometry_sha256']:
            raise AssertionError('The same seed and parameters produced different reference geometry')
        if any(not message.startswith('AssetWarning ') for message in a['cga_messages']):
            raise AssertionError(a['cga_messages'])
        stable.append(dict(case=case, triangles=a['triangles'], sha256=a['geometry_sha256'],
                           warnings=a['cga_messages'], height_above_provider_m=a['height_above_provider_m']))
    report = dict(**revision(), status='pass', analytic_controls=controls, real_extrusions=real,
                  deterministic_candler_cases=stable,
                  scope='PRT adapter geometry and determinism; no own architecture or visual acceptance claim.')
    output = Path('build/experiments/buildings/esri-validation.json')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(status='pass', analytic_controls=len(controls), real_footprints=len(real),
                          deterministic_cases=len(stable), output=str(output))))


if __name__ == '__main__':
    main()
