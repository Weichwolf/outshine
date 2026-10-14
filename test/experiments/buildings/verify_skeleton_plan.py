"""Analytic volumes, native closed caps, real-source coverage and bounded SDK partitions."""

import argparse
import json
from pathlib import Path
import resource

import numpy as np
import shapely
from shapely import affinity

from evidence import revision
from esri_skeleton_oracle import reference
from outshine_ref.buildings.modelling import Face, Shape, enu_to_cga, prepare_hip
from samples import read_site
from verify_modelling_roof_shed import measure
from verify_scope_library import costs


def caps(plan, value=3., mode='byHeight'):
    vertices = plan.lifted_vertices(value, mode); lookup = {tuple(point): index for index, point in enumerate(plan.positions_m)}
    faces = []
    for facet in plan.facets:
        ids = facet.node_indices
        if not shapely.is_ccw(facet.polygon.exterior):
            ids = ids[::-1]
        faces.append(Face(ids, tags=frozenset({'roof.top'}), material=3))
    rings = [tuple(lookup[tuple(point)] for point in np.asarray(ring.coords)[:-1])[::-1]
             for ring in (plan.footprint.exterior, *plan.footprint.interiors)]
    faces.append(Face(rings[0], tuple(rings[1:]), tags=frozenset({'roof.bottom'}), material=3))
    return Shape.from_faces(enu_to_cga(vertices), faces)


def verify(plan, value=3., mode='byHeight'):
    shape = caps(plan, value, mode); mesh = shape.emit()
    volume = sum(f.polygon.area*(np.asarray(f.gradient)@np.asarray(f.polygon.centroid.coords[0])+f.intercept_m)
                 for f in plan.facets)*plan.factor(value, mode)
    np.testing.assert_allclose(measure(mesh), volume, atol=1e-7, rtol=1e-9)
    np.testing.assert_allclose([np.linalg.norm(f.gradient) for f in plan.facets], 1., atol=1e-12)
    union = shapely.union_all([f.polygon for f in plan.facets])
    assert union.symmetric_difference(plan.footprint).area <= max(1e-7, plan.footprint.area*1e-10)
    assert len(plan.facets) == sum(len(r.coords)-1 for r in (plan.footprint.exterior, *plan.footprint.interiors))
    return dict(facets=len(plan.facets), nodes=len(plan.positions_m), volume_m3=volume, triangles=len(mesh['faces']),
                method=plan.method, events=plan.events, correction_m=plan.maximum_refinement_m)


def controls():
    polygons = [shapely.box(0, 0, 12, 5), shapely.box(0, 0, 5, 5), shapely.Polygon([(0, 0), (12, 0), (8, 7)]),
                shapely.Polygon([(0, 0), (12, 0), (10, 7), (2, 6)]),
                shapely.Polygon([(0, 0), (10, 0), (10, 3), (5, 3), (5, 8), (0, 8)]),
                shapely.Polygon([(0, 0), (10, 0), (10, 8), (0, 8)], [[(2, 2), (2, 6), (7, 6), (7, 2)]])]
    rows = []
    for index, polygon in enumerate(polygons):
        for angle in (0., 31., 90.):
            plan = prepare_hip(affinity.rotate(polygon, angle), 'skeleton')
            for value, mode in ((3., 'byHeight'), (25., 'byAngle')):
                rows.append(verify(plan, value, mode) | dict(control=index, angle=angle, mode=mode))
    return rows, polygons


def real_inputs():
    rows = []
    for site in ('Vienna', 'Tokyo', 'Rosenheim'):
        samples, _ = read_site(site)
        selected = sorted(samples, key=lambda s: len(s['polygon'].exterior.coords), reverse=True)[:16]
        for sample in selected:
            try:
                plan = prepare_hip(sample['polygon'])
            except (ValueError, RuntimeError) as error:
                row = dict(status='rejected', reason=type(error).__name__+': '+str(error))
            else:
                row = verify(plan) | dict(status='generated',fallback_reason=plan.fallback_reason)
            rows.append(row | dict(source_id=sample['id'], source_sha256=sample['source_sha256'],
                                   source_holes=len(sample['polygon'].interiors)))
    return rows


def sdk_comparison(polygons):
    rows = []
    for polygon in polygons:
        if polygon.interiors:
            continue
        rounded = shapely.Polygon(np.asarray(polygon.exterior.coords, dtype=np.float32))
        oracle = reference(rounded)
        plan = prepare_hip(rounded)
        expected = [shapely.Polygon(points) for points in oracle['facets']]
        actual = [affinity.translate(f.polygon, *plan.origin) for f in plan.facets]
        assert len(expected) == len(actual)
        matches = [min(range(len(expected)), key=lambda i: p.hausdorff_distance(expected[i])) for p in actual]
        assert len(set(matches)) == len(actual)
        error = max(p.hausdorff_distance(expected[i]) for p, i in zip(actual, matches, strict=True))
        assert error <= 3e-5, 'Float32 SDK skeleton differs from the independent Python partition'
        rows.append(dict(facets=len(actual),maximum_boundary_error_m=error,binary_sha256=oracle['binary_sha256'],
                         scope=oracle['scope'],input='Same Float32-rounded outline, no hole removal', oracle=oracle))
    return rows


def comparisons():
    rows = []
    for angle in (0., 31., 90.):
        polygon = affinity.rotate(shapely.box(0, 0, 12, 5), angle)
        a, b = prepare_hip(polygon, 'rectangle'), prepare_hip(polygon, 'skeleton')
        np.testing.assert_allclose(verify(a)['volume_m3'], verify(b)['volume_m3'], atol=1e-8)
        rows.append(dict(angle=angle, **costs(dict(rectangle=lambda: prepare_hip(polygon, 'rectangle'),
                                                skeleton=lambda: prepare_hip(polygon, 'skeleton')))))
    return rows


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/skeleton-plan-verification.json'))
    args = parser.parse_args(); rows, polygons = controls(); real = real_inputs()
    sample = next(s for s in read_site('Vienna')[0] if s['id']=='Vienna:1210000:12')
    offset = np.asarray(sample['polygon'].exterior.coords)[0]
    polygons.append(affinity.translate(sample['polygon'], -offset[0], -offset[1]))
    result = revision() | dict(status='pass', controls=rows, real_cases=real, sdk_comparisons=sdk_comparison(polygons),
                              comparisons=comparisons(), process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*1024,
                              measurement_scope='21 alternating warm plan preparations include validation/refinement; tracked allocations exclude untracked native heaps. '
                              'Linux process high-water RSS includes all sources/meshes/checks, not per-call peak. No rendering/GPU/frames.',
                              limits='Plan and closed-cap checks only; arbitrary source-native UV0-9, Shape.roof_hip, overhang/even/trim and isolated CGA rule remain open.')
    args.output.parent.mkdir(parents=True, exist_ok=True); args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(status='pass',controls=len(rows),sources=len(real),generated=sum(r['status']=='generated' for r in real),
                          rejected=[r for r in real if r['status']=='rejected'],sdk_cases=len(result['sdk_comparisons']))))
