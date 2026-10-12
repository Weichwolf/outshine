from pathlib import Path
import json
import sys
import time

import numpy as np
import shapely
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

sys.path.insert(0,'test/experiments/transport')
from transport_driving_network import DrivingNetwork
from transport_directed_routes import directed_module_routes
from transport_shared_frames import frame_key
from transport_spline_routes import spline_controls,curve_bounds,evaluate_controls,spline_route
from transport_route_fairing import fair_route
from transport_vehicle_sweep import body_outlines
from transport_building_clearance import plot_geometry
from transport_junction_surfaces import same_island_winding

ROOT=Path('build/experiments/transport')
OUTPUT=ROOT/'curve-deficits'
VEHICLE=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
REFERENCE=json.loads((ROOT/'pose-frames/shared-frames.json').read_text())


def inspect_candidate(entry,exit,route,surface,error,spacing,weight):
    if spacing is None:
        points=shapely.get_coordinates(route)
    else:
        count=max(2,int(np.ceil(route.length/spacing))+1)
        if count>66:
            return None
        points=shapely.get_coordinates(shapely.line_interpolate_point(route,np.linspace(0,route.length,count)))
    if len(points)>66:
        return None
    points[[0,-1]]=[entry['position'],exit['position']]
    controls=spline_controls(fair_route(points,weight),entry,exit)
    if controls is None:
        return None
    owners=np.repeat(np.arange(len(controls)),33)
    stations=np.tile(np.linspace(0,1,33),len(controls))
    xy=evaluate_controls(controls[owners],stations)
    velocity=evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
    acceleration=evaluate_controls(20*np.diff(controls[owners],n=2,axis=1),stations)
    speed=np.linalg.norm(velocity,axis=1)
    cross=velocity[:,0]*acceleration[:,1]-velocity[:,1]*acceleration[:,0]
    actual=np.divide(np.abs(cross),speed**3,out=np.full(len(speed),np.inf),where=speed>1e-10)
    _,_,bounds=curve_bounds(controls)
    bodies=shapely.polygons(body_outlines(xy,np.arctan2(velocity[:,1],velocity[:,0]),
                           VEHICLE['widthM']*.5,VEHICLE['frontFromRearAxleM'],VEHICLE['rearOverhangM']))
    covered=shapely.covers(surface,bodies)&(shapely.distance(bodies,shapely.boundary(surface))>=error)
    path,note=spline_route(entry,exit,route,surface,VEHICLE,error,spacing=spacing,fairing_weight=weight)
    line=shapely.LineString(xy)
    winding=same_island_winding(line,route,surface)
    report=dict(spacing=spacing,weight=weight,controls=len(controls),sampled_poses=len(xy),
                sampled_curvature=float(actual.max()),curvature_bound=float(bounds.max()),
                sampled_body_failures=int(np.sum(~covered)),simple=bool(line.is_simple),winding=bool(winding),
                continuous_accepted=path is not None and winding,continuous_note=note)
    return report,(xy,bodies,covered)


def run():
    fig,axes=plt.subplots(1,3,figsize=(15,7),constrained_layout=True)
    records=[]
    for ax,(name,module,entry_index,exit_index) in zip(axes,[('Wien',4524,0,1),
            ('ZuerichHauptbahnhof',8207,0,1),('ZuerichHauptbahnhof',6958,0,2)]):
        began=time.perf_counter()
        network=DrivingNetwork(ROOT,name,VEHICLE)
        reference=next(p for p in REFERENCE['places'] if p['place']==name)
        network.frames={frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in reference['frames']}
        report,view=network.record(module)
        shape,surface,poses,_,_=view
        routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
        entry,exit=poses[entry_index],poses[exit_index]
        route=routes[entry_index,exit_index]
        results=[]
        for spacing in (8.,4.,12.,16.,None):
            for weight in (0.,.1,1.,10.,100.):
                result=inspect_candidate(entry,exit,route,surface,network.curbs['arc_error_m'],spacing,weight)
                if result is not None:
                    results.append(result)
        best=min(results,key=lambda r:(r[0]['sampled_body_failures']/r[0]['sampled_poses']+
                     max(0,r[0]['sampled_curvature']*VEHICLE['minimumRearAxleRadiusM']-1)))
        sampled_possible=[r for r,_ in results if r['sampled_body_failures']==0 and r['simple'] and
                          r['winding'] and r['sampled_curvature']*VEHICLE['minimumRearAxleRadiusM']<=1]
        record=dict(place=name,module=module,entry=entry_index,exit=exit_index,route_length_m=route.length,
                    source_vertices=len(route.coords),candidates=[r for r,_ in results],
                    sampled_possible=len(sampled_possible),continuous_accepted=sum(r['continuous_accepted'] for r,_ in results),
                    total_ms=(time.perf_counter()-began)*1000)
        records.append(record)
        plot_geometry(ax,[surface],'#c7cdc3')
        plot_geometry(ax,[shape],'#73988855')
        ax.plot(*shapely.get_coordinates(route).T,color='#734c99',linestyle=':',label='source axis')
        xy,bodies,covered=best[1]
        ax.plot(*xy.T,color='#db9425',linewidth=1,label='diagnostic spline')
        bad=np.flatnonzero(~covered)
        if len(bad):
            plot_geometry(ax,list(bodies[bad[np.linspace(0,len(bad)-1,min(8,len(bad))).astype(int)]]),'#ce4a3f55')
        for pose in (entry,exit):
            ax.arrow(*pose['position'],*(2*pose['heading']),width=.08,head_width=.5,color='#254f43')
        ax.set(aspect='equal',facecolor='#f1f0e8',xlabel='East [m]',ylabel='North [m]',
               title=f'{name} #{module}\n{len(sampled_possible)}/25 sampled candidates plausible; no sampling proof')
        ax.legend(fontsize=8)
        print(json.dumps({k:v for k,v in record.items() if k!='candidates'}),flush=True)
    fig.savefig(OUTPUT/'corridor-diagnosis.png',dpi=140)
    plt.close(fig)
    (OUTPUT/'probe.json').write_text(json.dumps(records,indent=2)+'\n')


if __name__=='__main__':
    run()
