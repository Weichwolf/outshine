"""Explicitly export pinned real roof plans for native cross-host integration."""

import argparse
import json
from pathlib import Path

from compare_roof_oracle import cases
from evidence import revision
from outshine_ref.buildings.modelling import prepare_hip
from samples import read_site


def record(row, receipts):
    polygon = row['polygon']
    plan = prepare_hip(polygon)
    site = row['id'].split(':')[0]
    return dict(
        id=row['id'], source_sha256=row['source_sha256'],
        source=receipts[site],
        outer=list(polygon.exterior.coords)[:-1],
        holes=[list(r.coords)[:-1] for r in polygon.interiors],
        frame=dict(axes='XY = east/north, Z = roof rise, metres',
                   origin_xy_m=plan.origin, terrain='not sampled'),
        expected=dict(method=plan.method, events=plan.events,
                      maximum_refinement_m=plan.maximum_refinement_m,
                      positions_local_xy_m=plan.positions_m.tolist(),
                      wavefront_distances_m=plan.distances_m.tolist(),
                      cases=[dict(mode=mode, value=value, height_factor=plan.factor(value, mode))
                             for mode, value in (('byHeight', 3.), ('byAngle', 25.))],
                      facets=[dict(nodes=f.node_indices, source_edge=f.boundary,
                                   gradient=f.gradient, intercept_m=f.intercept_m)
                              for f in plan.facets]))


def export(output):
    receipts = {}
    for site in ('Vienna', 'Tokyo', 'Rosenheim'):
        _, source = read_site(site)
        receipts[site] = dict(url=source['receipt']['url'],
                              origin_lat_lon=source['origin_lat_lon'],
                              projection=source['projection'])
    rows = [record(row, receipts) for row in cases() if not row['control']]
    data = dict(schema='outshine.hip-plan-reference/1', producer=revision(),
                attribution='OpenStreetMap contributors; OpenFreeMap vector tiles; ODbL',
                scope='Original eaves and standard roof plans; no even, overhang, terrain, UV or SDK interior tessellation oracle.',
                comparison=dict(position_tolerance_m=5e-5, height_tolerance_m=5e-5,
                                wavefront_plane_tolerance_m=1e-7,
                                note='Compare geometric coverage and heights, not incidental node order or collinear subdivisions.'),
                records=rows)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(data, separators=(',', ':'))+'\n')
    print(f'{len(rows)} plans; {output.stat().st_size} bytes; {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path(__file__).parent/'reference/hip-plans.json')
    export(parser.parse_args().output)
