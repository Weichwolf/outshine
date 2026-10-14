"""Compare actual pinned SDK roof nodes/face boundaries with the independent Python plans."""

import argparse
import json
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree
import shapely

from evidence import revision
from esri_roof_oracle import reference
from outshine_ref.buildings.modelling import prepare_hip
from outshine_ref.buildings.modelling.hip_surface import prepare_even
from samples import read_site


def cases():
    controls=[('rectangle',shapely.box(0,0,12,5)),('trapezoid',shapely.Polygon([(0,0),(12,0),(10,7),(2,6)])),
              ('L',shapely.Polygon([(0,0),(10,0),(10,3),(5,3),(5,8),(0,8)])),
              ('U',shapely.Polygon([(0,0),(12,0),(12,8),(9,8),(9,3),(3,3),(3,8),(0,8)])),
              ('courtyard',shapely.Polygon([(0,0),(10,0),(10,8),(0,8)], [[(2,2),(2,6),(7,6),(7,2)]]))]
    rows=[dict(id=name,polygon=poly,control=True) for name,poly in controls]
    for site in ('Vienna','Tokyo','Rosenheim'):
        samples,_=read_site(site)
        rows.extend(dict(id=s['id'],polygon=s['polygon'],source_sha256=s['source_sha256'],control=False)
                    for s in sorted(samples,key=lambda s:len(s['polygon'].exterior.coords),reverse=True)[:16])
    return rows


def compare(row,value,mode,even):
    polygon=row['polygon'];plan=prepare_hip(polygon);field=prepare_even(plan,mode) if even else plan
    actual=reference(polygon,value,mode,even=even);points=np.asarray(actual['positions_m'])
    mine=np.c_[plan.positions_m+plan.origin,field.distances_m*field.factor(value,mode)]
    first=cKDTree(mine).query(points)[0];second=cKDTree(points).query(mine)[0]
    error=float(max(first.max(),second.max()));tolerance=1e-7 if row['control'] else 5e-5
    edge_error=None;status='pass' if error<=tolerance else 'mismatch'
    if status=='mismatch':
        own_edges=[(a,b) for f in plan.facets for a,b in zip(f.node_indices,f.node_indices[1:]+f.node_indices[:1])]
        sdk_edges=[(a,b) for f in actual['faces'] for a,b in zip(f,f[1:]+f[:1])]
        edge_error=max(point_edges(mine,points[np.asarray(sdk_edges)]),point_edges(points,mine[np.asarray(own_edges)]))
        if edge_error<=tolerance:status='subdivision-equivalent'
    return dict(id=row['id'],mode=mode,value=value,even=even,status=status,
                source_sha256=row.get('source_sha256'),sdk_nodes=len(points),own_nodes=len(mine),sdk_faces=len(actual['faces']),own_faces=len(plan.facets),
                maximum_node_distance_m=error,maximum_node_to_edge_distance_m=edge_error,tolerance_m=tolerance,sdk_geometry=actual)


def point_edges(points,segments):
    first=segments[:,0];delta=segments[:,1]-first;length=(delta*delta).sum(axis=1)
    first,delta,length=first[length>0],delta[length>0],length[length>0]
    maximum=0.
    for point in points:
        station=np.clip(((point-first)*delta).sum(axis=1)/length,0.,1.)
        maximum=max(maximum,float(np.linalg.norm(point-first-station[:,None]*delta,axis=1).min()))
    return maximum


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/roof-kernel-comparison.json'))
    args=parser.parse_args();results=[]
    for row in cases():
        for value,mode in ((3.,'byHeight'),(25.,'byAngle')):
            for even in (False,True):
                try:result=compare(row,value,mode,even)
                except Exception as error:result=dict(id=row['id'],mode=mode,even=even,status='error',reason=str(error))
                results.append(result)
                if result['status'] not in ('pass','subdivision-equivalent'):print(json.dumps({k:v for k,v in result.items() if k!='sdk_geometry'}),flush=True)
    record=revision()|dict(results=results,passed=sum(r['status'] in ('pass','subdivision-equivalent') for r in results),cases=len(results),
                          scope='Executable SDK geometry kernel only; node sets/edge subdivisions and facet count, not Processor UV/material/bottom/scope/trim, face interior interpolation or SDK nonplanar tessellation. Mismatches retained; one nearly simultaneous event remains open.')
    args.output.write_text(json.dumps(record,indent=2)+'\n');print('passed',record['passed'],'of',record['cases'])
