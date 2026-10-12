"""Independent axis, centre, ownership and executable PRT controls for the first modelling API."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import pyprt
import shapely

from esri_catalog import EXAMPLES,SDK
from esri_oracle import shape as prt_shape
from evidence import revision
from modelling_scope import Frame,Relative,Scope
from modelling_shape import Shape


def near(actual,expected):
    if not np.allclose(actual,expected,rtol=0.,atol=1e-10):
        raise AssertionError(f'Scope result differs: {actual} != {expected}')


def reject(call):
    try:call()
    except ValueError:return
    raise AssertionError('Invalid modelling input was accepted')


def scope_controls():
    rows=[]
    near(Scope().r(90,0,0).matrix[:3,:3]@np.array([1.,2.,3.]),[1.,-3.,2.])
    near(Scope().r(0,90,0).matrix[:3,:3]@np.array([1.,2.,3.]),[3.,2.,-1.])
    near(Scope().r(0,0,90).matrix[:3,:3]@np.array([1.,2.,3.]),[-2.,1.,3.])
    scope=Scope().r(90,0,0).r(0,90,0).r(-90,0,0)
    near(scope.matrix[:3,:3]@np.array([1.,2.,3.]),[3.,2.,-1.])
    rows.append('noncommuting Euler additions and the three positive coordinate axes')
    scope=Scope(size=(2.,3.,4.)).r(0,90,0).t(Relative(.5),2.,Relative(.5))
    near(scope.origin,[2.,2.,-1.]);near(scope.size,[2.,3.,4.])
    near(scope.s(Relative(-2.),0.,7.).size,[-4.,0.,7.])
    rows.append('rotated translation, dimensional relative values, signed and zero scope sizes')
    for angles in ((0.,0.,0.),(20.,35.,-17.),(90.,0.,90.)):
        for size in ((2.,3.,4.),(-2.,3.,4.),(2.,0.,4.)):
            scope=Scope(position=(1.,-4.,7.),orientation=angles,size=size,
                        pivot=Frame((3.,2.,-1.),(35.,-20.,12.)),object_frame=Frame((100.,10.,-80.),(-5.,70.,9.)))
            rotated=scope.r(31.,-22.,63.,'scopeCenter')
            near(rotated.centre,scope.centre)
            near(scope.r(31.,-22.,63.).origin,scope.origin)
    rows.append('nine translated/tilted object and pivot cases retain centre or origin as requested')
    scope=Scope(position=(1.,2.,3.),pivot=Frame((5.,0.,0.),(0.,90.,0.)),object_frame=Frame((10.,20.,30.)))
    near(scope.translate('abs','world',1.,2.,3.).origin,[1.,2.,3.])
    near(scope.translate('abs','object',1.,2.,3.).origin,[11.,22.,33.])
    near(scope.translate('abs','pivot',1.,2.,3.).origin,[18.,22.,29.])
    near(scope.translate('rel','world',1.,2.,3.).origin,scope.origin+[1.,2.,3.])
    near(scope.translate('rel','object',1.,2.,3.).origin,scope.origin+[1.,2.,3.])
    near(scope.translate('rel','pivot',1.,2.,3.).origin,scope.origin+[3.,2.,-1.])
    near(scope.translate('rel','scope',1.,2.,3.).origin,scope.t(1.,2.,3.).origin)
    near(scope.translate('abs','scope',1.,2.,3.).origin,scope.t(1.,2.,3.).origin)
    rows.append('absolute and relative translation in all four coordinate systems')
    for array in (scope.matrix,scope.origin,scope.axes,scope.centre,scope.pivot.axes):
        if array.flags.writeable:raise AssertionError('Cached frame data can be changed in place')
    for value in (float('inf'),float('nan'),True,'3'):
        reject(lambda:Scope().t(value,0.,0.))
    reject(lambda:Scope(position=(1.,2.)));reject(lambda:Scope().r(0.,0.,0.,'unknown'))
    reject(lambda:Scope().translate('unknown','world',0.,0.,0.))
    reject(lambda:Scope().translate('abs','unknown',0.,0.,0.))
    rows.append('nonfinite, nonnumeric and unknown-selector inputs rejected')
    return rows


def geometry_controls():
    vertices=np.array([[2.,3.,4.],[4.,3.,4.],[2.,6.,8.]])
    mesh=dict(vertices=vertices,faces=np.array([[0,1,2]],dtype=np.int32),materials=np.array([1],dtype=np.uint8),
              uv=np.array([[0.,0.],[2.,0.],[0.,3.]]))
    shape=Shape.from_mesh(mesh);vertices[:]=999.
    near(shape.emit()['vertices'],[[2.,3.,4.],[4.,3.,4.],[2.,6.,8.]])
    scaled=shape.s(Relative(2.),Relative(.5),Relative(-1.))
    near(scaled.emit()['vertices'],[[2.,3.,4.],[6.,3.,4.],[2.,4.5,0.]])
    if scaled.geometry is not shape.geometry:raise AssertionError('Scope operation copied immutable asset geometry')
    if scaled.emit()['uv'] is not shape.geometry.fields['uv']:raise AssertionError('Scope operation rewrote texture coordinates')
    for array in (shape.geometry.unit_vertices,*shape.geometry.fields.values()):
        if array.flags.writeable:raise AssertionError('Geometry payload is mutable')
    points=np.array([[0.,0.,0.],[1.,0.,0.],[0.,1.,-1.]])
    mesh=dict(vertices=points,faces=np.array([[0,1,2]],dtype=np.int32),normals=np.tile([0.,1.,1.],(3,1)))
    moved=Shape.from_mesh(mesh).s(2.,3.,4.).r(0.,90.,0.).emit()
    edges=moved['vertices'][1:]-moved['vertices'][0]
    near(edges@moved['normals'][0],[0.,0.]);near(np.linalg.norm(moved['normals'],axis=1),[1.,1.,1.])
    reject(lambda:Shape.from_mesh(mesh).s(2.,0.,4.).emit())
    reject(lambda:Shape.from_mesh(dict(vertices=points,faces=np.array([[0.,1.,2.]]))))
    reject(lambda:Shape.from_mesh(dict(vertices=points,faces=np.array([[0,1,3]]))))
    flat=Shape.from_mesh(dict(vertices=np.array([[0.,2.,0.],[3.,2.,0.],[0.,2.,4.]]),faces=np.array([[0,1,2]])))
    near(flat.s(6.,9.,8.).emit()['vertices'],[[0.,2.,0.],[6.,2.,0.],[0.,2.,8.]])
    empty=Shape.from_mesh(dict(vertices=np.empty((0,3)),faces=np.empty((0,3),dtype=np.int32)))
    if empty.t(1.,2.,3.).emit()['vertices'].shape!=(0,3):raise AssertionError('Empty shape fabricated geometry')
    return ['owned immutable geometry and shared scope derivatives','metric/UV preservation and signed rescaling',
            'authored normal inverse transpose; singular authored-normal transforms explicitly pending',
            'flat and empty assets, invalid triangle indices and index types']


def prt_controls():
    rows=[]
    polygon=shapely.Polygon([(0.,0.),(8.,0.),(8.,5.),(0.,5.)],
                            [[(2.,1.),(2.,3.),(5.,3.),(5.,1.)]])
    sample=dict(id='scope-translation-control',polygon=polygon,source_sha256='analytic',properties=dict(height=8.,min_height=2.))
    initial,_=prt_shape(sample);generator=pyprt.ModelGenerator([initial])
    rule=EXAMPLES/'data/translateModel.rpk'
    options={'emitGeometry':True,'emitReport':True,'triangulate':True}
    models=generator.generate_model([dict(vec_x=0.,vec_y=0.,vec_z=0.)],str(rule),'com.esri.pyprt.PyEncoder',options)
    if len(models)!=1 or models[0].get_cga_errors():raise AssertionError('Translation oracle did not produce one valid model')
    baseline=np.asarray(models[0].get_vertices()).reshape(-1,3)
    indices=np.asarray(models[0].get_indices()).reshape(-1,3)
    if not len(indices) or not len(baseline) or not np.isfinite(baseline).all():raise AssertionError('Translation oracle returned no finite surface')
    triangles=baseline[indices]
    area=np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1).sum()/2
    near(area,polygon.area)
    for delta in ((3.,4.,-2.),(-7.,-3.,6.)):
        attrs=dict(zip(('vec_x','vec_y','vec_z'),delta))
        model=generator.generate_model([attrs],str(rule),'com.esri.pyprt.PyEncoder',options)[0]
        actual=np.asarray(model.get_vertices()).reshape(-1,3)
        if model.get_cga_errors():raise AssertionError(model.get_cga_errors())
        near(actual,baseline+delta)
        rows.append(dict(delta_m=delta,vertices=len(actual),prt_version=pyprt.get_api_version(),
                         rule_sha256=hashlib.sha256(rule.read_bytes()).hexdigest(),
                         scope='Bundled translation rule on an unrotated input; no arbitrary CGA rotation oracle claim'))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-scope-validation.json'))
    args=parser.parse_args()
    result=revision()|dict(status='pass',scope_controls=scope_controls(),geometry_controls=geometry_controls(),prt_controls=prt_controls(),
                         implemented=['t absolute/relative dimensions','r origin/centre','s absolute/relative dimensions',
                                      'translate abs/rel in scope/pivot/object/world'],
                         pending=['polygon/edge/vertex shape constructors','singular authored-normal transforms',
                                  'isolated PRT rotation/scale rule controls','undocumented floating-scale selector semantics'])
    docs=SDK/'doc/html/cgaref/cgareference'
    result['sdk_docs_sha256']={name:hashlib.sha256((docs/(name+'.html')).read_bytes()).hexdigest()
                               for name in ('attr_scope','attr_pivot','op_t','op_r','op_s','op_translate')}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',contract_groups=len(result['scope_controls'])+len(result['geometry_controls']),prt_controls=len(result['prt_controls']))))


if __name__=='__main__':main()
