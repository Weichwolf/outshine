"""Compare source-biased steering queues without changing steering or acceptance budgets."""

import argparse
import inspect
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from geos_triangulation import triangles_of
from infrastructure_cut_setback_experiment import digest,implementation_files
from infrastructure_junction_surfaces import same_island_winding
import infrastructure_steering_guide as steering
from infrastructure_turn_roadbeds import roadbed_lateral_reach,bounded_voids


def ray_steps(a,b,markers):
    a=a[:,None,:]-markers;b=b[:,None,:]-markers
    cross=a[:,:,0]*b[:,:,1]-a[:,:,1]*b[:,:,0]
    return (((a[:,:,1]<=0)&(b[:,:,1]>0)&(cross>0)).astype(int)-
            ((a[:,:,1]>0)&(b[:,:,1]<=0)&(cross<0)).astype(int))


def island_markers(surface,domain,region,triangles=False):
    holes=[shapely.Polygon(ring) for part in shapely.get_parts(surface) for ring in part.interiors]
    cores=shapely.difference(np.array(holes,dtype=object),domain)
    parts=shapely.get_parts(cores)
    parts=parts[np.isin(shapely.get_type_id(parts),[3,6])]
    if triangles:
        inner=shapely.intersection(shapely.buffer(np.array(holes,dtype=object),-.5),region)
        cells=triangles_of(shapely.union_all(inner)) if np.any(~shapely.is_empty(inner)) else np.array([],dtype=object)
        parts=np.r_[parts,cells[shapely.area(cells)>=.25]]
    points=shapely.point_on_surface(np.r_[np.array(holes,dtype=object),parts])
    return shapely.get_coordinates(points[shapely.covers(region,points)])


def source_hint(source,markers,domain,target,radius,bias):
    xy=shapely.get_coordinates(source);vectors=np.diff(xy,axis=0)
    length=np.linalg.norm(vectors,axis=1);assert np.all(length>0)
    starts=np.r_[0.,np.cumsum(length)];total=starts[-1]
    prefix=np.vstack([np.zeros((1,len(markers)),dtype=int),np.cumsum(ray_steps(xy[:-1],xy[1:],markers),axis=0)])[:-1]
    direction=vectors/length[:,None]
    def score(pose,signature):
        p=pose[:2];euclidean=np.linalg.norm(p-target)
        fraction=np.clip(np.sum((p-xy[:-1])*vectors,axis=1)/length**2,0,1)
        projected=xy[:-1]+fraction[:,None]*vectors
        signatures=prefix+ray_steps(xy[:-1],projected,markers)+ray_steps(projected,np.broadcast_to(p,projected.shape),markers)
        eligible=np.all(signatures==signature,axis=1)
        links=shapely.linestrings(np.stack([np.broadcast_to(p,projected.shape),projected],axis=1))
        eligible&=shapely.covers(domain,links)
        forward=np.array([np.cos(pose[2]),np.sin(pose[2])])
        cost=np.linalg.norm(projected-p,axis=1)+total-starts[:-1]-fraction*length
        cost+=radius*(1-direction@forward)
        estimate=float(cost[eligible].min()) if eligible.any() else euclidean+total
        return (1-bias)*euclidean+bias*estimate
    return score


def variant(bias,core_markers,triangle_markers=False):
    code=inspect.getsource(steering.steering_guide)
    statement='wanted=crossing_signature(shapely.get_coordinates(source),markers)'
    assert code.count(statement)==1
    code=code.replace(statement,statement+'\n    hint=source_hint(source,markers,domain,exit["position"],radius,bias)')
    if core_markers:
        marker_filter='markers=markers[shapely.covers(region,shapely.points(markers))]'
        assert code.count(marker_filter)==1
        code=code.replace(marker_filter,'markers=island_markers(surface,domain,region,triangle_markers)')
    for old,new in (("np.linalg.norm(origin-exit['position'])",'hint(initial,zero)'),
                    ("np.linalg.norm(successor[:2]-exit['position'])",'hint(successor,signature)')):
        assert code.count(old)==1;code=code.replace(old,new)
    namespace=dict(steering.__dict__,source_hint=source_hint,island_markers=island_markers,bias=bias,
                   triangle_markers=triangle_markers)
    exec(compile(code,str(Path(__file__)),'exec'),namespace)
    return namespace['steering_guide']


def verify():
    source=shapely.LineString([[0.,0.],[10.,0.],[10.,10.]])
    floor=shapely.box(-5,-5,15,15);shapely.prepare(floor)
    hint=source_hint(source,np.empty((0,2)),floor,np.array([10.,10.]),4.,1.)
    assert hint(np.array([0.,0.,0.]),())==20.
    assert hint(np.array([10.,10.,np.pi/2]),())==0.
    points=np.array([[-1,-1],[1,-1],[1,1],[-1,1],[-1,-1]])
    markers=np.array([[0.,0.],[2.,0.]])
    assert np.array_equal(ray_steps(points[:-1],points[1:],markers).sum(axis=0),steering.crossing_signature(points,markers))
    return dict(source_distance_and_target=True,ray_prefixes_match=True,optimality_claim=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--bias',type=float,nargs='+',default=[.5,1.])
    parser.add_argument('--maximum-states',type=int,default=20000)
    parser.add_argument('--core-markers',action='store_true')
    parser.add_argument('--triangle-markers',action='store_true')
    parser.add_argument('--island-inset',type=float)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);checks=verify();rows=[]
    if any(not 0<=b<=1 for b in args.bias):parser.error('bias must be between zero and one')
    if args.island_inset is not None and not 0<args.island_inset<float('inf'):parser.error('island inset must be finite and positive')
    for path in args.input:
        data=json.loads(path.read_text());vehicle,error=data['vehicle'],data['error_m']
        poses=[{k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit')]
        floor,reserved,source=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
        if args.island_inset is not None:
            reserved=reserved.union(shapely.union_all(shapely.buffer(bounded_voids(floor),-args.island_inset)))
            data['reservations']=reserved.wkb.hex()
            (args.output/(path.stem+'-prepared.json')).write_text(json.dumps(data)+'\n')
        domain=floor.buffer(roadbed_lateral_reach(vehicle)-2*error).difference(reserved.difference(floor).buffer(2*error))
        for bias in args.bias:
            begin=time.perf_counter()
            guide,note=variant(bias,args.core_markers or args.triangle_markers,args.triangle_markers)(*poses,source,floor,domain,vehicle,maximum_states=args.maximum_states,
                                   heading_bins=64,cell=.225,radius_scale=1.)
            row=dict(input=str(path),input_sha256=digest(path),bias=bias,core_markers=args.core_markers,
                     triangle_markers=args.triangle_markers,island_inset_m=args.island_inset,guide_found=guide is not None,
                     elapsed_ms=(time.perf_counter()-begin)*1000,search=note)
            if guide is not None:
                assert same_island_winding(guide,source,floor)
                output=args.output/f'{path.stem}-{bias:g}-guide.json'
                output.write_text(json.dumps(dict(path=shapely.get_coordinates(guide).tolist()))+'\n')
                row.update(guide=str(output),length_m=guide.length)
            rows.append(row);print(json.dumps(row),flush=True)
    sources=implementation_files(Path(__file__))+[Path(steering.__file__).with_name('geos_triangulation.py').resolve()]
    result=dict(checks=checks,cases=rows,maximum_states=args.maximum_states,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        scope='Source-biased sampled steering guides; unchanged state/body budgets, no shortest-path guarantee or native curve claim.')
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
