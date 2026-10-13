from pathlib import Path
import json
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,'test/experiments/transport')
from transport_driving_network import DrivingNetwork
from transport_directed_routes import directed_module_routes
from transport_shared_frames import frame_key
from transport_vehicle_sweep import body_outlines


def boundary_index(surface):
    parts=shapely.get_parts(shapely.boundary(surface))
    coordinates,rings=shapely.get_coordinates(parts,return_index=True)
    keep=(rings[1:]==rings[:-1])&np.any(coordinates[1:]!=coordinates[:-1],axis=1)
    segments=shapely.linestrings(np.stack((coordinates[:-1][keep],coordinates[1:][keep]),axis=1))
    return shapely.STRtree(segments),len(segments)


def indexed_distance(tree,points):
    indices,distances=tree.query_nearest(points,all_matches=False,return_distance=True)
    result=np.full(len(points),np.nan)
    result[indices[0]]=distances
    return result


def clearance_shortfall(surface,boundary,tree,points,margin):
    inside=shapely.covers(surface,points)
    interior=np.flatnonzero(inside)
    indices=tree.query(points[interior],predicate='dwithin',distance=margin)
    active=~inside
    active[interior[indices[0]]]=True
    result=np.zeros(len(points))
    distance=shapely.distance(points[active],boundary)
    result[active]=np.maximum(margin-np.where(inside[active],distance,-distance),0)
    return result,int(active.sum())


def boxed_shortfall(surface,boundary,tree,points,margin):
    inside=shapely.covers(surface,points)
    interior=np.flatnonzero(inside)
    coordinates=shapely.get_coordinates(points[interior])
    low=np.nextafter(coordinates-margin,-np.inf)
    high=np.nextafter(coordinates+margin,np.inf)
    boxes=shapely.box(low[:,0],low[:,1],high[:,0],high[:,1])
    indices=tree.query(boxes)
    active=~inside
    active[interior[indices[0]]]=True
    result=np.zeros(len(points))
    distance=shapely.distance(points[active],boundary)
    result[active]=np.maximum(margin-np.where(inside[active],distance,-distance),0)
    return result,int(active.sum())


def run():
    root=Path('build/experiments/transport')
    reference=json.loads((root/'hermite-final/hermite-routes.json').read_text())
    vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
    results=[]
    for name,module,a,b in [('Wien',1202,15,29),('ZuerichHauptbahnhof',1137,13,10),('BaselBadischerBahnhof',721,3,37)]:
        source=next(p for p in reference['places'] if p['place']==name)
        network=DrivingNetwork(root,name,vehicle)
        network.frames={frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in source['frames']}
        _,view=network.record(module)
        shape,surface,poses,_,_=view
        routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
        route=routes[a,b]
        xy=shapely.get_coordinates(shapely.line_interpolate_point(route,np.linspace(0,route.length,256)))
        direction=np.gradient(xy,axis=0)
        outlines=body_outlines(xy,np.arctan2(direction[:,1],direction[:,0]),vehicle['widthM']*.5,
                              vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
        points=shapely.points(outlines.reshape(-1,2))
        boundary=shapely.boundary(surface)
        shapely.prepare(surface)
        started=time.perf_counter()
        tree,count=boundary_index(surface)
        build_ms=(time.perf_counter()-started)*1000
        exact=shapely.distance(points,boundary)
        indexed=indexed_distance(tree,points)
        assert np.allclose(exact,indexed,atol=1e-12,rtol=0)
        totals={}
        for method in ('whole_boundary','indexed_segments','whole_boundary','indexed_segments'):
            started=time.perf_counter()
            for _ in range(20):
                values=shapely.distance(points,boundary) if method=='whole_boundary' else indexed_distance(tree,points)
            totals.setdefault(method,[]).append((time.perf_counter()-started)*1000/20)
            assert np.allclose(values,exact,atol=1e-12,rtol=0)
        row=dict(place=name,module=module,points=len(points),boundary_segments=count,index_build_ms=build_ms,
                 whole_boundary_ms=min(totals['whole_boundary']),indexed_segments_ms=min(totals['indexed_segments']),
                 maximum_difference_m=float(np.max(np.abs(indexed-exact))))
        row['speedup']=row['whole_boundary_ms']/row['indexed_segments_ms']
        margin=network.curbs['arc_error_m']*3
        expected=np.maximum(margin-np.where(shapely.covers(surface,points),exact,-exact),0)
        started=time.perf_counter()
        for _ in range(20):
            shortfall,active=clearance_shortfall(surface,boundary,tree,points,margin)
        gate_ms=(time.perf_counter()-started)*1000/20
        assert np.array_equal(shortfall,expected)
        row.update(gated_shortfall_ms=gate_ms,active_points=active,all_points=len(points),
                   gated_speedup=row['whole_boundary_ms']/gate_ms,bitwise_equal_shortfall=True)
        started=time.perf_counter()
        for _ in range(20):
            boxed,active=boxed_shortfall(surface,boundary,tree,points,margin)
        box_ms=(time.perf_counter()-started)*1000/20
        assert np.array_equal(boxed,expected)
        row.update(boxed_shortfall_ms=box_ms,boxed_active_points=active,
                   boxed_speedup=row['whole_boundary_ms']/box_ms,boxed_bitwise_equal=True)
        results.append(row)
        print(json.dumps(row),flush=True)
    (root/'distance-costs/comparison.json').write_text(json.dumps(results,indent=2)+'\n')


if __name__=='__main__':
    run()
