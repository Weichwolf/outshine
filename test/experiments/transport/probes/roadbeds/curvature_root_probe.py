import sys,json,time
from pathlib import Path
import numpy as np
import shapely
from scipy.optimize import brentq
sys.path.insert(0,'test/experiments/transport')
from transport_clothoids import quadrature,weighted_sum,wrapped
from transport_curvature_paths import heading_progress,curvature_extrema,curvature_rate_bound,sample_curvature_paths
from transport_vehicle_sweep import turn_body_check

r=json.loads(Path('build/experiments/transport/roadbeds/ZuerichHauptbahnhof-102-roadbeds.json').read_text());v=r['vehicle']
points,weights=quadrature(96);grid=np.linspace(-6*np.pi,6*np.pi,769)
for row in r['trials']:
    poses=[]
    for outline in row['endpoint_bodies']:
        a=np.asarray(outline);h=a[1]-a[0];h/=np.linalg.norm(h)
        poses.append(dict(position=(a[0]+a[3])*.5+h*v['rearOverhangM'],heading=h))
    a,b=poses;initial=np.arctan2(*a['heading'][::-1]);final=np.arctan2(*b['heading'][::-1])
    displacement=b['position']-a['position'];chord=np.linalg.norm(displacement);direction=np.arctan2(*displacement[::-1])
    delta=float(wrapped(final-initial));relative=float(wrapped(initial-direction))
    def residual(amplitude):
        return float(weighted_sum(np.sin(relative+heading_progress(points,delta,amplitude)),weights))
    values=np.array([residual(x) for x in grid]);roots=[]
    for lo,hi in zip(np.flatnonzero(values[:-1]*values[1:]<0),np.flatnonzero(values[:-1]*values[1:]<0)+1):
        amplitude=brentq(residual,grid[lo],grid[hi],xtol=1e-13)
        forward=float(weighted_sum(np.cos(relative+heading_progress(points,delta,amplitude)),weights))
        if forward<=1e-9:continue
        length=chord/forward;d=np.array([delta]);amp=np.array([amplitude]);l=np.array([length])
        curvature=curvature_extrema(d,amp,l);radius=1/curvature[0]
        fit=dict(start=a['position'][None,:],end=b['position'][None,:],start_heading=np.array([initial]),delta=d,amplitude=amp,
                 length_m=l,maximum_curvature_per_m=curvature,maximum_curvature_rate_per_m2=curvature_rate_bound(d,amp,l))
        xy=sample_curvature_paths(fit,np.linspace(0,1,257))[0][0];path=shapely.LineString(xy)
        accepted=radius>=v['minimumRearAxleRadiusM'] and path.is_simple
        row_result=dict(amplitude=amplitude,length_m=length,minimum_radius_m=radius,simple=path.is_simple,
                        endpoint_error_m=float(np.linalg.norm(xy[-1]-b['position'])),accepted=bool(accepted),bounds=path.bounds)
        if accepted:
            lo=xy.min(axis=0)-10;hi=xy.max(axis=0)+10
            body,_=turn_body_check(shapely.box(*lo,*hi),fit,a['position'],b['position'],'G2-polynomial',v)
            row_result['body']=body
        roots.append(row_result)
    print(json.dumps(dict(pair=[row['entry'],row['exit']],roots=roots)),flush=True)
