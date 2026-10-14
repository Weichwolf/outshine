"""Native source-triangle UV surfaces, conformal roof closure, frames and appearance ownership."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from modelling_hip_surface_probe import local,surface_heights
from outshine_ref.buildings.modelling import Frame, Shape
from outshine_ref.buildings.modelling.faces import ROOF_TAGS, plane
from samples import read_site
from verify_modelling_roof_shed import source


def measure(product):
    points = product['vertices']; triangles = points[product['faces']]
    first, last = triangles[:,1]-triangles[:,0], triangles[:,2]-triangles[:,0]
    normals = np.cross(first, last)
    products = abs(first[:,[1,2,0]]*last[:,[2,0,1]])+abs(first[:,[2,0,1]]*last[:,[1,2,0]])
    uncertainty = 8*np.finfo(float).eps*np.linalg.norm(products,axis=1)
    areas = np.linalg.norm(normals,axis=1)
    assert np.all(areas > uncertainty), 'Roof triangle normal is numerically unresolved'
    _, identifiers = np.unique(points,axis=0,return_inverse=True)
    faces = identifiers[product['faces']]
    edges = np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]])
    _, inverse, counts = np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True,return_counts=True)
    assert np.all(counts == 2), 'Roof has an exact open or nonmanifold edge'
    signs = np.where(edges[:,0] < edges[:,1],1,-1)
    assert np.all(np.bincount(inverse,weights=signs) == 0), 'Roof triangle winding is inconsistent'
    centred = triangles.astype(np.longdouble)-triangles.mean(axis=(0,1)).astype(np.longdouble)
    volume = float((centred[:,0]*np.cross(centred[:,1],centred[:,2])).sum()/6)
    return volume, dict(minimum_double_area_m2=float(areas.min()),
                        maximum_normal_relative_uncertainty=float((uncertainty/areas).max()))


def uv_surface(original, result):
    before, after = local(original), local(result); checked = 0
    channels = [key for key in before if key == 'uv' or key.startswith('uv') and key[2:].isdigit()]
    samples = np.array([[1.,0.,0.], [0.,1.,0.], [0.,0.,1.], [.2,.3,.5]])
    for owner, face in enumerate(original.geometry.polygons):
        normal, _, _, _ = plane(before['vertices'], face); origin = before['vertices'][face.outer[0]]
        edge = before['vertices'][face.outer[1]]-origin; edge /= np.linalg.norm(edge)
        basis = np.array([edge, np.cross(normal, edge)])
        ids = before['faces'][before['parts'] == owner]; triangles = before['vertices'][ids]
        poly = shapely.polygons((triangles-origin)@basis.T)
        selection = np.asarray(result.roof_info.source_faces)[after['parts']] == owner
        output = after['faces'][selection]; points = np.einsum('si,tij->tsj', samples, after['vertices'][output]).reshape(-1,3)
        points -= ((points-origin)@normal)[:,None]*normal
        pairs = shapely.STRtree(poly).query(shapely.points((points-origin)@basis.T), predicate='dwithin', distance=1e-8)
        candidate = triangles[pairs[1]]; a, b = candidate[:,1]-candidate[:,0], candidate[:,2]-candidate[:,0]
        delta = points[pairs[0]]-candidate[:,0]
        aa, ab, bb = np.einsum('ij,ij->i',a,a), np.einsum('ij,ij->i',a,b), np.einsum('ij,ij->i',b,b)
        da, db = np.einsum('ij,ij->i',delta,a), np.einsum('ij,ij->i',delta,b)
        denominator = aa*bb-ab*ab
        u, v = (bb*da-ab*db)/denominator, (aa*db-ab*da)/denominator
        bary = np.c_[1-u-v,u,v]; valid = bary.min(axis=1) >= -1e-7
        for channel in channels:
            expected = np.einsum('pi,pij->pj', bary, before[channel][ids[pairs[1]]])
            actual = np.einsum('si,tij->tsj', samples, after[channel][output]).reshape(-1,2)[pairs[0]]
            valid &= np.all(np.isclose(actual, expected, atol=1e-7, rtol=1e-9), axis=1)
        passed = np.zeros(len(points),dtype=bool); passed[pairs[0][valid]] = True
        assert passed.all(), 'Hip cut changed a native source UV interpolation domain'
        checked += len(points)*len(channels)
    return checked


def verify(shape, value=3., mode='byHeight', even=False):
    before = shape.emit(); roof = shape.roof_hip(value, mode,even=even); mesh = local(roof)
    expected = 0.
    for plan,surface in zip(roof.roof_info.plans,roof.roof_info.surfaces,strict=True):
        facets=plan.facets if surface is None else surface.facets
        expected += sum(f.polygon.area*(np.asarray(f.gradient)@np.asarray(f.polygon.centroid.coords[0])+f.intercept_m)
                        for f in facets)*(plan if surface is None else surface).factor(value,mode)
        if surface is not None:
            for group in surface.ridge_groups:
                _,unique=np.unique(plan.positions_m[list(group)],axis=0,return_index=True)
                values=plan.distances_m[np.asarray(group)[unique]]
                np.testing.assert_allclose(surface.distances_m[list(group)],values.max() if mode=='byHeight' else values.mean(),atol=1e-12)
            np.testing.assert_array_equal(surface.distances_m[plan.distances_m==0],0.)
    volume, quality = measure(mesh)
    np.testing.assert_allclose(volume,expected,atol=1e-7,rtol=1e-9)
    checked = uv_surface(shape,roof)
    quality|=surface_heights(shape,roof,value,mode)
    triangles=mesh['vertices'][mesh['faces']]
    normals=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    for owner,face in enumerate(shape.geometry.polygons):
        normal=plane(local(shape)['vertices'],face)[0]
        selection=np.asarray(roof.roof_info.source_faces)[mesh['parts']]==owner
        top=np.asarray(['roof.top' in face.tags for face in roof.geometry.polygons])[mesh['parts']]
        assert np.all(normals[selection&top]@normal>0), 'Hip top normal faces into its source plane'
        assert np.all(normals[selection&~top]@normal<0), 'Hip bottom normal faces away from its source plane'
    assert roof.geometry.group_names == shape.geometry.group_names and roof.geometry.material_defs == shape.geometry.material_defs
    assert roof.material == shape.material and roof.textures == shape.textures and roof.projections == shape.projections
    assert roof.comp_info == shape.comp_info
    for index, face in enumerate(roof.geometry.polygons):
        original = shape.geometry.polygons[roof.roof_info.source_faces[index]]
        assert face.material == original.material and face.group == original.group
        assert original.tags-ROOF_TAGS.keys() <= face.tags and len(face.tags&ROOF_TAGS.keys()) == 1
    for key in before:
        np.testing.assert_array_equal(shape.emit()[key],before[key])
    normal = plane(local(shape)['vertices'], shape.geometry.polygons[0])[0]
    np.testing.assert_allclose(roof.scope.axes[:,1],shape.scope.object_frame.axes@normal,atol=1e-10)
    return dict(triangles=len(mesh['faces']),volume_m3=volume,expected_volume_m3=expected,uv_values_checked=checked,**quality,
                source_faces=len(shape.geometry.polygons),top_faces=sum('roof.top' in f.tags for f in roof.geometry.polygons),
                overlay_cells=roof.roof_info.overlay_cells,overlay_precision_m=roof.roof_info.overlay_precision_m,
                maximum_shared_height_adjustment_m=roof.roof_info.maximum_shared_height_adjustment_m)


def controls(even=False):
    polygons = [shapely.box(0,0,12,5),shapely.Polygon([(0,0),(10,0),(10,3),(5,3),(5,8),(0,8)]),
                shapely.Polygon([(0,0),(10,0),(10,8),(0,8)], [[(2,2),(2,6),(7,6),(7,2)]]),
                shapely.Polygon([(0,0),(12,0),(10,7),(2,6)])]
    rows = []
    for polygon in polygons:
        original = source(polygon)
        for shape in (original,original.r(17,31,23),original.s(-12,0,9).r(17,31,23),
                      replace(original,scope=replace(original.scope,pivot=Frame((7,-3,8),(7,11,19)),
                                                     object_frame=Frame((1e8,200,-300),(13,17,23))))):
            for value,mode in ((3.,'byHeight'),(25.,'byAngle')):
                rows.append(verify(shape,value,mode,even))
    base = source(polygons[0]); mesh = base.emit()
    changed = dict(mesh); changed['uv'] = mesh['uv'].copy(); changed['uv'][mesh['faces'][0]] += [7.,3.]
    seam = Shape.from_mesh(changed,base.scope,base.geometry.polygons).with_metadata(group_names=dict(base.geometry.group_names),material_defs=dict(base.geometry.material_defs))
    rows.append(verify(seam,even=even))
    authored = Shape.from_mesh(mesh|dict(normals=np.tile([0.,1.,0.],(len(mesh['vertices']),1))),base.scope,base.geometry.polygons)
    roof = authored.roof_hip(3.,'byHeight'); assert roof.roof_info.authored_normals_rebuilt and 'normals' not in roof.emit()
    checked_roof=authored.roof_hip(3.,'byHeight',even=even);changed=local(checked_roof)
    changed['vertices']=changed['vertices'].copy()
    top=np.asarray(['roof.top' in f.tags for f in checked_roof.geometry.polygons])[changed['parts']]
    changed['vertices'][changed['faces'][top][0,0]] += [0.,.25,0.]
    try:surface_heights(authored,checked_roof,3.,'byHeight',changed)
    except AssertionError:pass
    else:raise AssertionError('Corrupted hip height surface was not detected')
    corrupted = roof.emit(); corrupted['uv7'] = corrupted['uv7']+.25
    wrong = replace(roof,geometry=Shape.from_mesh(corrupted,roof.scope,roof.geometry.polygons).geometry)
    try:uv_surface(authored,wrong)
    except AssertionError:pass
    else:raise AssertionError('Corrupted hip UV7 was not detected')
    return rows


def real_inputs(even=False):
    rows=[]
    for site in ('Vienna','Tokyo','Rosenheim'):
        samples,_=read_site(site)
        for sample in sorted(samples,key=lambda s:len(s['polygon'].exterior.coords),reverse=True)[:16]:
            try:
                row=verify(source(sample['polygon']),even=even)
            except Exception as error:
                raise AssertionError(f"Hip source failed: {sample['id']}") from error
            rows.append(row|dict(source_id=sample['id'],source_sha256=sample['source_sha256']))
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-roof-hip-verification.json'))
    parser.add_argument('--even',action='store_true')
    args=parser.parse_args(); control=controls(args.even); real=real_inputs(args.even)
    record=revision()|dict(status='pass',even=args.even,controls=control,real_cases=real,
                          scope='Independent Gram-barycentric UV0-9 samples, metric volumes, source ownership and native closure; not SDK UV/overhang/even/trim or native frame proof')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',controls=len(control),real_cases=len(real),uv_values_checked=sum(r['uv_values_checked'] for r in control+real))))
