"""Check exterior finish against analytic wall areas, rotated courtyards and storey geometry."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from shapely.affinity import rotate

from building_mesh import solid
from building_surface_roles import apply,INTERIOR_WALL
from building_vertical import mesh
from evidence import revision
from opening_details import attach,expand,slots
from showcase_setbacks import house


def controls():
    rows=[]
    shapes=[shapely.box(0,0,18,14),shapely.Polygon([(0,0),(26,0),(26,22),(0,22)],
        holes=[[(9,7),(9,15),(17,15),(17,7)]])]
    for index,outline in enumerate(shapes):
        for angle in (0.,31.):
            outline=rotate(outline,angle);storage=[],[],[],[]
            wall=outline.difference(outline.buffer(-.24,join_style='mitre'))
            solid(storage,wall,0.,3.,1)
            product={key:np.asarray(value) for key,value in zip(('vertices','faces','materials','parts'),storage,strict=True)}
            product['levels']=np.zeros(len(product['faces']),dtype=np.uint8)
            plan=dict(storeys=1,storey_plans=[dict(footprint=outline)])
            result,info=apply(plan,product)
            assert abs(info['exterior_area_m2']-outline.length*3.)<1e-6
            expected_inner=outline.buffer(-.24,join_style='mitre').length*3.+2*wall.area
            assert abs(info['interior_and_reveal_area_m2']-expected_inner)<1e-6
            for key in product:
                if key!='materials':np.testing.assert_array_equal(result[key],product[key])
            assert np.all(product['materials']==1)
            rows.append(dict(case=index,rotation_deg=angle,**info))
    plan=house();body=attach(mesh(plan),expand(slots(plan)));result,info=apply(plan,body)
    for key in body:
        if key!='materials':np.testing.assert_array_equal(result[key],body[key])
    selected=body['materials']!=1
    np.testing.assert_array_equal(result['materials'][selected],body['materials'][selected])
    assert set(np.unique(result['materials'][~selected]))=={1,INTERIOR_WALL}
    for floor,current in enumerate(plan['storey_plans']):
        exterior=(result['materials']==1)&(result['levels']==floor)
        xyz=result['vertices'][result['faces'][exterior]].mean(axis=1)
        assert len(xyz)>0 and np.all(shapely.distance(shapely.points(xyz[:,:2]),current['footprint'].boundary)<1e-7)
        if floor:
            assert np.any(shapely.distance(shapely.points(xyz[:,:2]),plan['footprint'].boundary)>.5)
    try:apply(plan,result)
    except ValueError:pass
    else:raise AssertionError('Repeated role assignment lost the declared source material')
    rows.append(dict(case='stepped-house',**info));return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/surface-role-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),limits='Constructive vertical walls only; native material/LOD integration remains open')
    args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=len(record['cases']))))
