"""Documented width examples, analytic area integrals and real courtyard partitions."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from shapely.affinity import rotate,translate

from evidence import revision
from modelling_extrude_probe import fixtures
from outshine_ref.buildings.modelling.scope import Relative
from outshine_ref.buildings.modelling.split_area import TriangleArea
from outshine_ref.buildings.modelling.plan_split import clipped,split_plan
from outshine_ref.buildings.modelling.split_sizes import Floating,Part,allocations


def near(actual,expected,tolerance=1e-8):
    if not np.allclose(actual,expected,rtol=1e-10,atol=tolerance):raise AssertionError((actual,expected))


def allocation_controls():
    examples=[('relative',[Relative(.5),Relative(.1),Relative(.2)],False,[5.,1.,2.]),
        ('floating',[Floating(.5),Floating(.1),Floating(.2)],False,[6.25,1.25,2.5]),
        ('fixed-floating',[3.3,Floating(3),5.],False,[3.3,1.7,5.]),
        ('oversized',[Relative(.5),Relative(.6),3.],False,[5.,5.]),
        ('repeat-fixed',[2.,1.],True,[2.,1.,2.,1.,2.,1.,1.]),
        ('repeat-floating',[Floating(2),Floating(1)],True,[20/9,10/9]*3),
        ('repeat-relative',[Relative(.1),Relative(.1)],True,[1.]*10),
        ('repeat-mixed',[1.,Floating(2)],True,[1.,7/3]*3),
        ('floating-oversized',[7.,Floating(2),6.],False,[7.,3.])]
    for label,sizes,repeat,expected in examples:
        spans=allocations(10.,[Part(size,str(i)) for i,size in enumerate(sizes)],repeat)
        near([span.stop-span.start for span in spans],expected)
        assert [span.index for span in spans]==list(range(len(spans)))
        assert all(span.total==len(spans) and span.label==str(span.pattern_index) for span in spans)
    for call in [lambda:allocations(1.,[Part(0.)],True),lambda:Part(-1.),lambda:Part(Floating(-1.)),
                 lambda:allocations(1.,[Part(.0001)],True,max_parts=10),lambda:allocations(float('inf'),[Part(1.)])]:
        try:call()
        except ValueError:pass
        else:raise AssertionError('Invalid or unbounded split accepted')
    return [item[0] for item in examples]


def cumulative_controls():
    # A right triangle has A(x)=8x-x² on [0,4]; vertical edges exercise equal knots.
    triangle=shapely.Polygon([(0.,0.),(4.,0.),(0.,8.)]);integral=TriangleArea(triangle,0)
    for x in np.linspace(0.,4.,17):
        target=8*x-x*x;near(integral(x),target);near(integral.inverse(target,0.,4.),x)
    fixtures_=[triangle,shapely.box(0,0,4,8),shapely.MultiPolygon([shapely.box(0,0,2,2),shapely.box(4,0,6,2)])]
    for polygon in fixtures_:
        for axis in (0,1):
            integral=TriangleArea(polygon,axis);low,high=polygon.bounds[axis],polygon.bounds[axis+2]
            for x in np.linspace(low,high,33):near(integral(x),clipped(polygon,axis,low,x).area)
    return dict(analytic_right_triangle=17,geos_cumulative_comparisons=198)


def geometry_controls():
    rows=[];pattern=[Part(Floating(1.),str(i)) for i in range(4)]
    for label,polygon,_,_,source_sha in fixtures():
        original=polygon.wkb
        for axis in ('x','y'):
            products={method:split_plan(polygon,axis,pattern,area=True,method=method) for method in ('triangles','geos')}
            a,b=products.values()
            bisected=split_plan(polygon,axis,pattern,area=True,method='triangles-bisect')
            for left,right in zip(a,bisected,strict=True):near(left.geometry.symmetric_difference(right.geometry).area,0.,tolerance=1e-7)
            for left,right in zip(a,b,strict=True):
                near(left.geometry.area,polygon.area/4,tolerance=1e-7)
                near(left.geometry.symmetric_difference(right.geometry).area,0.,tolerance=1e-7)
                near(left.cuts,right.cuts,tolerance=1e-7)
            geometries=[part.geometry for part in a];combined=shapely.union_all(geometries)
            near(combined.symmetric_difference(polygon).area,0.,tolerance=1e-7)
            near(sum(part.area for part in geometries)-combined.area,0.,tolerance=1e-7)
            nested=[piece.geometry for part in a for piece in split_plan(part.geometry,'y' if axis=='x' else 'x',pattern[:2],area=True)]
            near([piece.area for piece in nested],[polygon.area/8]*8,tolerance=1e-7)
            assert polygon.wkb==original
            rows.append(dict(label=label,axis=axis,source_sha256=source_sha,area_m2=polygon.area,
                             fragments=sum(len(list(shapely.get_parts(part.geometry))) for part in a),nested_parts=len(nested)))
    disconnected=shapely.MultiPolygon([shapely.box(0,0,2,2),shapely.box(4,0,6,2)])
    children=split_plan(disconnected,'x',[Part(2.),Part(2.),Part(2.)],adjust=False)
    assert children[1].geometry.is_empty and children[1].scope_bounds==(2.,0.,4.,2.)
    assert children[2].span.index==2 and children[2].span.total==3
    rotated=translate(rotate(disconnected,31.,origin=(0,0)),xoff=100.,yoff=-20.)
    children=split_plan(rotated,'x',pattern,area=True)
    near([part.geometry.area for part in children],[2.]*4)
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/split-plan-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',allocations=allocation_controls(),
        cumulative=cumulative_controls(),geometry=geometry_controls(),
        limits='Planar 2D prototype; no native mesh/UV cuts, nested pattern allocation, isolated PRT split rule or runtime LOD admission')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',allocation_examples=len(record['allocations']),source_axis_cases=len(record['geometry']))))


if __name__=='__main__':main()
