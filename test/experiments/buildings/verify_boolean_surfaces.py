"""Independent GEOS slab integrals, directed topology and source UV checks for CSG."""

import argparse
from dataclasses import dataclass,replace
import importlib.metadata
import json
from pathlib import Path
import resource
import time

import numpy as np
import shapely

from boolean_surface_probe import boolean
from compare_roof_oracle import cases
from evidence import revision
from outshine_ref.buildings.modelling import Frame
from verify_modelling_roof_shed import source


@dataclass(frozen=True)
class Prism:
    polygon: object
    low: float
    high: float

    def shape(self):
        result=source(self.polygon)
        for channel in range(1,10):result=result.delete_uv(channel)
        result=result.extrude(self.high-self.low).translate('rel','world',0.,self.low,0.)
        for channel in range(1,10):result=result.copy_uv(0,channel).scale_uv(channel,channel+1,channel+2)
        return result


def reference(a,b,operation):
    levels=sorted(set([a.low,a.high,b.low,b.high]));previous=shapely.Polygon();volume=area=0.
    for low,high in zip(levels[:-1],levels[1:],strict=True):
        z=(low+high)/2
        first=a.polygon if a.low<z<a.high else shapely.Polygon()
        second=b.polygon if b.low<z<b.high else shapely.Polygon()
        current=getattr(first,dict(union='union',subtract='difference',intersect='intersection')[operation])(second)
        if current.area==0.:current=shapely.Polygon()
        volume+=current.area*(high-low);area+=current.length*(high-low)+previous.symmetric_difference(current).area
        previous=current
    return volume,area+previous.area


def measure(product):
    mesh=product.mesh;triangles=mesh['vertices'][mesh['faces']]
    if not len(triangles):return 0.,0.
    vectors=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]);lengths=np.linalg.norm(vectors,axis=1)
    uncertainty=8*np.finfo(float).eps*np.linalg.norm(abs((triangles[:,1]-triangles[:,0])[:,[1,2,0]]*(triangles[:,2]-triangles[:,0])[:,[2,0,1]])+
        abs((triangles[:,1]-triangles[:,0])[:,[2,0,1]]*(triangles[:,2]-triangles[:,0])[:,[1,2,0]]),axis=1)
    if np.any(lengths<=uncertainty):raise ValueError('Boolean triangle normal is unresolved')
    faces=product.roots[mesh['faces']]
    edges=np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]])
    _,inverse,counts=np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True,return_counts=True)
    if not np.all(counts==2):raise ValueError('Boolean result has an open/nonmanifold logical edge')
    if np.any(np.bincount(inverse,weights=np.where(edges[:,0]<edges[:,1],1,-1))):raise ValueError('Boolean boundary orientation fails')
    shifted=triangles.astype(np.longdouble)-triangles.mean(axis=(0,1)).astype(np.longdouble)
    volume=float((shifted[:,0]*np.cross(shifted[:,1],shifted[:,2])).sum()/6)
    return volume,float(lengths.sum()/2)


def check_uv(product):
    from boolean_source_coordinates import rational_weights
    mesh=product.mesh;checked=0
    for owner in range(2):
        selected=product.operands==owner;before=product.sources[owner].mesh
        corners=before['vertices'][before['faces'][product.source_triangles[selected]]]
        actual_points=mesh['vertices'][mesh['faces'][selected]]
        if not len(corners):continue
        weights=np.asarray([rational_weights(triangle,points) for triangle,points in zip(corners,actual_points,strict=True)])
        if weights.min()<-1e-7:raise ValueError('Boolean relation extrapolates beyond a source triangle')
        np.testing.assert_allclose(np.einsum('nki,nij->nkj',weights,corners),actual_points,atol=1e-8,rtol=1e-10)
        for channel in range(10):
            name='uv'+str(channel) if channel else 'uv'
            values=before[name][before['faces'][product.source_triangles[selected]]]
            expected=np.einsum('nki,nij->nkj',weights,values)
            np.testing.assert_allclose(mesh[name][mesh['faces'][selected]],expected,atol=1e-7,rtol=1e-9)
            checked+=expected.size
    return checked


def controls():
    base=Prism(shapely.box(0,0,12,8),0.,5.)
    yield 'overlap',base,Prism(shapely.box(6,3,18,11),2.,8.)
    yield 'coplanar-overlap',base,Prism(shapely.box(6,3,18,11),0.,5.)
    yield 'identical',base,base
    yield 'face-contact',base,Prism(shapely.box(12,0,18,8),0.,5.)
    yield 'edge-contact',base,Prism(shapely.box(12,8,18,12),0.,5.)
    yield 'point-contact',base,Prism(shapely.box(12,8,18,12),5.,10.)
    yield 'disjoint',base,Prism(shapely.box(15,0,20,8),0.,5.)
    yield 'through-hole',base,Prism(shapely.box(2,2,6,6),-1.,6.)
    yield 'thin-passage',base,Prism(shapely.box(5.999,0,6.001,8),-1.,6.)
    yield 'nested',base,Prism(shapely.box(2,2,6,6),1.,4.)
    polygon=shapely.Polygon([(0,0),(16,0),(16,12),(0,12)],[[(3,3),(3,9),(12,9),(12,3)]])
    yield 'courtyard',Prism(polygon,0.,5.),Prism(shapely.box(6,0,9,12),2.,7.)
    l=shapely.Polygon([(0,0),(12,0),(12,4),(5,4),(5,9),(0,9)])
    yield 'concave',Prism(l,0.,5.),Prism(shapely.box(3,2,8,7),1.,8.)
    yield 'circular-tower',base,Prism(shapely.Point(10,4).buffer(3.,quad_segs=12),3.,12.)


def run(real,frame_mode='object'):
    rows=list(controls())
    if real:
        for row in cases():
            if row['control']:continue
            polygon=row['polygon'];x,y,a,b=polygon.bounds
            box=shapely.box(x+(a-x)*.3,y+(b-y)*.3,x+(a-x)*.7,y+(b-y)*.7)
            rows.append((row['id'],Prism(polygon,0.,5.),Prism(box,-1.,7.)))
    results=[];control_ids={row[0] for row in controls()}
    for identifier,a,b in rows:
      for tilted in ([False,True] if identifier in control_ids else [False]):
        for operation in ['union','subtract','intersect']:
            record=dict(id=identifier,operation=operation,frame='tilted' if tilted else 'identity')
            try:
                first,second=a.shape(),b.shape()
                if tilted:
                    placement=Frame((14.,23.,-17.),(17.,31.,23.))
                    first=replace(first,scope=replace(first.scope,object_frame=placement))
                    second=replace(second,scope=replace(second.scope,object_frame=placement))
                before=[first.emit(),second.emit()]
                started=time.perf_counter();product=boolean(first,second,operation,frame_mode=frame_mode);elapsed=(time.perf_counter()-started)*1000
                actual=measure(product);expected=reference(a,b,operation)
                np.testing.assert_allclose(actual,expected,atol=1e-7,rtol=1e-9)
                uv=check_uv(product)
                assert product.object_frame==(first.scope.object_frame if frame_mode=='object' else Frame())
                for input_source in product.sources:
                    points=input_source.mesh['vertices']@product.object_frame.axes.T+product.object_frame.position
                    np.testing.assert_allclose(points,input_source.shape.emit()['vertices'],atol=1e-8,rtol=0.)
                if tilted:
                    placed=dict(product.mesh)
                    placed['vertices']=placed['vertices']@product.object_frame.axes.T+product.object_frame.position
                    np.testing.assert_allclose(measure(replace(product,mesh=placed)),expected,atol=1e-7,rtol=1e-9)
                np.testing.assert_allclose(actual,[product.backend_volume_m3,product.backend_area_m2],atol=1e-7,rtol=1e-9)
                for shape,original in zip([first,second],before,strict=True):
                    for key,value in original.items():
                        if isinstance(value,np.ndarray):np.testing.assert_array_equal(shape.emit()[key],value)
                record.update(status='pass',generation_ms=elapsed,volume_m3=actual[0],area_m2=actual[1],
                              triangles=len(product.mesh['faces']),uv_values_checked=uv,zero_volume_contacts=product.zero_volume_contacts)
            except Exception as error:
                record.update(status='error',reason=str(error));print(record,flush=True)
            results.append(record)
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--real',action='store_true')
    parser.add_argument('--frame-mode',choices=['object','world'],default='object')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/boolean-surface-verification.json'))
    args=parser.parse_args();results=run(args.real,args.frame_mode)
    report=revision()|dict(results=results,cases=len(results),passed=sum(r['status']=='pass' for r in results),
        frame_mode=args.frame_mode,manifold_version=importlib.metadata.version('manifold3d'),process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Closed prism CSG experiment: independent GEOS slab volume/surface, logical directed closure, all native UV0–9 and triangle relations. No SDK equivalence, Shape material reconciliation, coplanar bool.A/B tags, open/2D semantics, full runtime budgets or recipe claim.')
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(report['passed'],'of',report['cases'],flush=True)
    raise SystemExit(0 if report['passed']==report['cases'] else 1)
