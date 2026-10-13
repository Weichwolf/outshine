"""Projected minimum-area frames: exhaustive hull-edge controls and real source footprints."""

import argparse
from dataclasses import replace
import hashlib
import json
from pathlib import Path
import resource

import numpy as np
import shapely
from shapely import affinity

from esri_ghidra import read_analysis
from esri_catalog import SDK
from evidence import revision
from outshine_ref.buildings.modelling import Frame, Shape
from outshine_ref.buildings.modelling.scope_bbox import SELECTORS, minimum_axis, referenced_vertices
from outshine_ref.buildings.modelling.scope_geometry import effective_size
from samples import read_site
from verify_modelling_roof_shed import source
from verify_scope_library import costs


def hull(points):
    geometry = shapely.convex_hull(shapely.multipoints(points))
    if geometry.geom_type == 'Polygon':
        return np.asarray(geometry.exterior.coords)[:-1]
    return np.asarray(geometry.coords)


def box_area(points, axis):
    perpendicular = np.array([-axis[1], axis[0]])
    size = np.ptp(points @ np.column_stack((axis, perpendicular)), axis=0)
    return float(np.prod(size))


def edge_rectangle(points):
    vertices = hull(points)
    edges = np.roll(vertices, -1, axis=0)-vertices
    lengths = np.linalg.norm(edges, axis=1)
    if not np.any(lengths):
        return 0., np.array([1., 0.])
    candidates = [(box_area(points, edge/length), edge/length)
                  for edge, length in zip(edges, lengths, strict=True) if length > 0]
    return min(candidates, key=lambda row: row[0])


def pca_axis(points):
    vertices = hull(points)
    centered = vertices-vertices.mean(axis=0)
    _, vectors = np.linalg.eigh(centered.T @ centered)
    return vectors[:, -1]


def verify(shape, selector):
    before = shape.emit()
    a, b = ('xyz'.index(axis) for axis in selector)
    c = 3-a-b
    used = referenced_vertices(shape.geometry)
    points = (shape.geometry.unit_vertices * effective_size(shape))[used][:, [a, b]]
    expected, _ = edge_rectangle(points)
    fitted = shape.align_scope_to_geometry_bbox(selector)
    mesh = fitted.emit()
    actual = fitted.scope.size[a] * fitted.scope.size[b]
    np.testing.assert_allclose(actual, expected, atol=1e-8, rtol=1e-9)
    np.testing.assert_allclose(mesh['vertices'], before['vertices'], atol=3e-8, rtol=0)
    np.testing.assert_allclose(fitted.scope.axes[:, c], shape.scope.axes[:, c], atol=1e-12)
    np.testing.assert_allclose(fitted.scope.origin, fitted.scope.pivot_origin, atol=1e-12)
    assert fitted.scope.size[a]+1e-8 >= fitted.scope.size[b]
    for key in before:
        if key == 'vertices':
            continue
        np.testing.assert_allclose(mesh[key], before[key], atol=1e-12, rtol=0)
        if not key.endswith('normals'):
            assert fitted.geometry.fields[key] is shape.geometry.fields[key]
    assert fitted.geometry.polygons is shape.geometry.polygons
    return dict(selector=selector, minimum_area_m2=expected, fitted_area_m2=actual,
                vertices=len(mesh['vertices']), maximum_position_error_m=float(abs(mesh['vertices']-before['vertices']).max()))


def controls():
    polygons = [shapely.box(0, 0, 12, 5), shapely.Polygon([(0, 0), (10, 0), (10, 3), (5, 3), (5, 8), (0, 8)]),
                shapely.Polygon([(0, 0), (10, 0), (10, 8), (0, 8)], [[(2, 2), (2, 6), (7, 6), (7, 2)]])]
    rows = []
    for polygon in polygons:
        for angle in (7., 31., 72.):
            flat = source(affinity.rotate(polygon, angle, origin=(0, 0)))
            mesh = flat.emit()
            flat = replace(flat, geometry=Shape.from_mesh(mesh | dict(normals=np.tile([0., 1., 0.], (len(mesh['vertices']), 1))),
                                                         flat.scope, flat.geometry.polygons).geometry)
            for framed in (flat, flat.s(-12., 0., 9.).r(17., 31., -23.),
                           replace(flat, scope=replace(flat.scope, pivot=Frame((7., -3., 8.), (7., 11., 19.)),
                                                      object_frame=Frame((1e8, 200., -300.), (13., 17., 23.))))):
                for selector in SELECTORS:
                    rows.append(verify(framed, selector))
    return rows


def real_inputs():
    rows, measurements = [], []
    for site in ('Vienna', 'Tokyo', 'Rosenheim'):
        samples, receipt = read_site(site)
        selected = sorted(samples, key=lambda row: len(row['polygon'].exterior.coords), reverse=True)[:32]
        for sample in selected:
            shape = Shape.from_polygon(sample['polygon'], 3.)
            rows.append(verify(shape, 'xz') | dict(source_id=sample['id'], source_sha256=sample['source_sha256']))
        for sample in selected[:3]:
            points = np.asarray(sample['polygon'].exterior.coords)[:-1]
            shape = Shape.from_polygon(sample['polygon'], 3.)
            exact, _ = edge_rectangle(points)
            pca = pca_axis(points)
            calls = dict(geos=lambda: minimum_axis(points), hull_edges=lambda: edge_rectangle(points), pca=lambda: pca_axis(points))
            measurements.append(dict(source_id=sample['id'], source_sha256=sample['source_sha256'], points=len(points),
                                     minimum_area_m2=exact, pca_area_m2=box_area(points, pca),
                                     area_contract='PCA is a measured approximation; only GEOS and hull-edge search meet minimum area',
                                     shape_fit=costs(dict(fit=lambda: shape.align_scope_to_geometry_bbox('xz')))['fit'],
                                     **costs(calls)))
    return rows, measurements


def reference():
    path = Path('build/experiments/buildings/esri-ghidra')
    analysis = read_analysis(path, 'scope bbox')
    functions = [row for row in analysis['selected_functions'] if row['address'] in ('003e7210', '004b3f00')]
    assert len(functions) == 2
    text = (path/'reference-003e7210.c').read_text()
    for word in ('Index2DSorter', 'getEulerAnglesFromBasisVectors', 'adjustScopeToGeometryBBox', 'copyAndTransformTrimPlanes'):
        assert word in text
    doc = SDK/'doc/html/cgaref/cgareference/op_alignScopeToGeometryBBox.html'
    return dict(binary_sha256=analysis['binary_sha256'], functions=functions, sdk_doc_sha256=hashlib.sha256(doc.read_bytes()).hexdigest(),
                scope='Static SDK hull/edge fit and official semantics; no executable isolated PRT alignment oracle',
                pending=['exact SDK area/length ties and sign thresholds', 'mixed-primitive SDK selection order', 'trim planes'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/scope-bbox-verification.json'))
    args = parser.parse_args()
    rows = controls()
    real, measurements = real_inputs()
    result = revision() | dict(status='pass', controls=rows, real_cases=real, measurements=measurements, reference=reference(),
                              shapely=shapely.__version__, geos=shapely.geos_version_string,
                              process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*1024,
                              process_peak_scope='Linux process high-water RSS for all source decodes/checks/measurements; not a per-call allocation peak',
                              measurement_scope='Alternating warm 2D direction selection including hull preparation; p50/p95, tracked allocations. '
                              'Excludes Shape rebake, source decode, rendering/GPU/native frames.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(status='pass',controls=len(rows),real_cases=len(real),measured_cases=len(measurements))))


if __name__ == '__main__':
    main()
