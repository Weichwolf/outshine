"""Scope-only rotation: stationary native geometry, indexed topology and reference evidence."""

import argparse
from dataclasses import replace
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely

from esri_ghidra import read_analysis
from esri_catalog import SDK
from evidence import revision
from outshine_ref.buildings.modelling import Asset, Frame, Geometry, Relative, Shape
from outshine_ref.buildings.modelling.scope import rotation
from outshine_ref.buildings.modelling.wire import Edge, Point
from verify_modelling_roof_shed import source
from verify_scope_library import costs


def world_fit(shape, angles):
    scope = shape.scope.r(*angles)
    mesh = shape.emit()
    local = (mesh['vertices']-scope.origin) @ scope.axes
    low, high = local.min(axis=0), local.max(axis=0)
    scope = replace(scope, position=tuple(np.asarray(scope.position)+rotation(scope.orientation)@low), size=tuple(high-low))
    geometry, _ = Geometry.from_mesh(mesh, scope, shape.geometry.polygons, shape.geometry.edges,
                                     shape.geometry.points, shape.geometry.group_names, shape.geometry.material_defs)
    return replace(shape, scope=scope, geometry=geometry)


def verify(shape, angles):
    before = shape.emit()
    changed = shape.rotate_scope(*angles)
    after = changed.emit()
    np.testing.assert_allclose(after['vertices'], before['vertices'], atol=2e-8, rtol=0)
    for name in before:
        if name == 'vertices':
            continue
        if name.endswith('normals'):
            np.testing.assert_allclose(after[name], before[name], atol=1e-12, rtol=0)
        else:
            np.testing.assert_array_equal(after[name], before[name])
            assert changed.geometry.fields[name] is shape.geometry.fields[name]
    for name in ('polygons', 'edges', 'points', 'group_names', 'material_defs'):
        assert getattr(changed.geometry, name) is getattr(shape.geometry, name)
    for name in ('material', 'textures', 'projections', 'comp_info', 'split_info', 'sweep_info', 'asset'):
        assert getattr(changed, name) is getattr(shape, name)
    np.testing.assert_allclose(changed.scope.orientation, np.asarray(shape.scope.orientation)+angles, atol=1e-12)
    local = changed.geometry.unit_vertices
    np.testing.assert_allclose(local.min(axis=0), [0, 0, 0], atol=1e-12)
    np.testing.assert_allclose(local.max(axis=0), np.where(np.asarray(changed.scope.size) == 0, 0, 1), atol=1e-12)
    return dict(angles_deg=angles, vertices=len(local), streams=sorted(after),
                maximum_position_error_m=float(np.max(abs(after['vertices']-before['vertices']))))


def controls():
    polygon = shapely.Polygon([(0, 0), (10, 0), (10, 8), (0, 8)], [[(2, 2), (2, 6), (7, 6), (7, 2)]])
    flat = source(polygon)
    mesh = flat.emit()
    authored = replace(flat, geometry=Shape.from_mesh(mesh | dict(normals=np.tile([0., 1., 0.], (len(mesh['vertices']), 1))),
                                                     flat.scope, flat.geometry.polygons).geometry)
    volume = Shape.from_polygon(polygon).extrude(4)
    wire = Shape.from_edges([(0., 0., 0.), (3., 0., 0.)], [Edge((0, 1))], [[0., 1., 0.]])
    points = Shape.from_points([(0., 0., 0.), (3., 0., 0.)], [Point((0,)), Point((1,))], [[0., 1., 0.]]*2)
    asset = Asset.from_shape(volume)
    inserted = volume.insert(asset).s(14., 2., 9.).r(0., 27., 0.)
    rows = []
    for original in (authored, volume, wire, points, inserted):
        for framed in (original, original.s(Relative(-1.2), Relative(.8), Relative(.9)),
                       replace(original, scope=replace(original.scope, pivot=Frame((7., -3., 8.), (7., 11., 19.)),
                                                      object_frame=Frame((1e8, 200., -300.), (13., 17., 23.))))):
            for angles in ((0., 31., 0.), (17., -28., 39.)):
                rows.append(verify(framed, angles))
    return rows


def benchmarks():
    rows = []
    root = Path('build/experiments/buildings/opening-inspections')
    for label in ('rectangle', 'vienna-compact', 'tokyo-compact'):
        path = root/('opening-'+label+'-mesh.npz')
        with np.load(path) as archive:
            mesh = dict(archive)
        shape = Shape.from_mesh(mesh)
        angles = (12., 25., -9.)
        old, new = world_fit(shape, angles).emit(), shape.rotate_scope(*angles).emit()
        np.testing.assert_allclose(old['vertices'], new['vertices'], atol=1e-7, rtol=0)
        for name in old:
            if name != 'vertices':
                np.testing.assert_allclose(old[name], new[name], atol=1e-12, rtol=0)
        times = costs(dict(world_fit=lambda: world_fit(shape, angles), pivot_fit=lambda: shape.rotate_scope(*angles)))
        rows.append(dict(input=str(path), input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                         vertices=len(mesh['vertices']), **times))
    return rows


def reference():
    path = Path('build/experiments/buildings/esri-ghidra')
    analysis = read_analysis(path, 'scope rotation')
    functions = [row for row in analysis['selected_functions'] if row['address'] in ('003e8890', '004c8b90')]
    assert len(functions) == 2
    text = (path/'reference-003e8890.c').read_text()
    assert 'param_4[6] = param_1 + dVar5' in text and 'isEmptyEv' in text and 'getBoundingBoxEv' in text
    doc = SDK/'doc/html/cgaref/cgareference/op_rotateScope.html'
    return dict(binary_sha256=analysis['binary_sha256'], functions=functions, sdk_doc_sha256=hashlib.sha256(doc.read_bytes()).hexdigest(),
                scope='Static SDK dispatcher/kernel and documented semantics; no executable isolated PRT rotateScope oracle',
                pending=['exact SDK tiny-coordinate normalization', 'trim planes', 'exact SDK instance treatment'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/scope-geometry-verification.json'))
    args = parser.parse_args()
    rows = controls()
    result = revision() | dict(status='pass', cases=rows, measurements=benchmarks(), reference=reference(),
                              measurement_scope='Alternating warm Python scope fitting, before emission; p50/p95 and tracked peak allocations. '
                              'Source decode, rendering/GPU and native frame time excluded.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(status='pass', cases=len(rows), measured_inputs=len(result['measurements']))))


if __name__ == '__main__':
    main()
