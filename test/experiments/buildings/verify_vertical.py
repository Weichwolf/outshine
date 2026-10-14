"""Inspect actual stair contacts, slab openings and sampled body clearance on the full mesh."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from building_mesh import mesh as first_mesh
from building_plan_cases import cases
from building_plan import FLOOR_M
from building_stair import FLIGHT_WIDTH_M, MAX_RISER_M, PLATE_M, TREAD_M, flight
from building_vertical import build, level, mesh
from evidence import revision
from verify_circulation import verify as verify_access
from verify_layout import closed_parts, occupancy,verify as verify_layout


class Columns:
    def __init__(self, product, region=None):
        triangles = product['vertices'][product['faces']]
        e1, e2 = triangles[:,1]-triangles[:,0], triangles[:,2]-triangles[:,0]
        area = e1[:,0]*e2[:,1]-e1[:,1]*e2[:,0]
        selected = abs(area)>1e-12
        if region is not None:
            lower,upper = region.min(axis=0)-.31,region.max(axis=0)+.31
            selected &= np.all(triangles[:,:,:2].max(axis=1)>=lower,axis=1)
            selected &= np.all(triangles[:,:,:2].min(axis=1)<=upper,axis=1)
        order = np.argsort(product['parts'][selected],kind='stable')
        self.triangles = triangles[selected][order]
        self.e1, self.e2, self.area = e1[selected][order],e2[selected][order],area[selected][order]
        parts = product['parts'][selected][order]
        self.starts = np.r_[0,np.flatnonzero(parts[1:] != parts[:-1])+1]

    def intervals(self, points):
        offset = points[:,None,:]-self.triangles[None,:,0,:2]
        b = (offset[:,:,0]*self.e2[:,1]-offset[:,:,1]*self.e2[:,0])/self.area
        c = (self.e1[:,0]*offset[:,:,1]-self.e1[:,1]*offset[:,:,0])/self.area
        inside = (b>=-1e-8)&(c>=-1e-8)&(b+c<=1+1e-8)
        height = self.triangles[:,0,2]+b*self.e1[:,2]+c*self.e2[:,2]
        low = np.minimum.reduceat(np.where(inside,height,np.inf),self.starts,axis=1)
        high = np.maximum.reduceat(np.where(inside,height,-np.inf),self.starts,axis=1)
        return low,high


def inspect_poses(columns, poses, feet=True):
    minimum = float('inf')
    angles = np.arange(16)*2*np.pi/16
    ring = np.r_[[[0.,0.]],np.c_[np.cos(angles),np.sin(angles)]]
    for first in range(0,len(poses),8):
        batch = poses[first:first+8]
        points = (batch[:,None,:2]+ring[None,:,:]*.30).reshape(-1,2)
        low,high = columns.intervals(points)
        z = np.repeat(batch[:,2],len(ring))[:,None]
        if np.any((high>z+.40+1e-6)&(low<z+1.90-1e-6)&(high-low>1e-7)):
            raise AssertionError('Actual mesh obstructs the sampled 0.30 m radius / 1.90 m high stair torso')
        above = np.min(np.where(low>=z+.40,low-z,np.inf))
        minimum = min(minimum,float(above))
        if feet:
            footpoints = (batch[:,None,:2]+ring[None,:,:]*.10).reshape(-1,2)
            lo,hi = columns.intervals(footpoints)
            support = np.max(np.where(hi<=z+1e-6,hi,-np.inf),axis=1)
            if np.any(abs(support-z[:,0])>1e-6) or np.any((lo<z-1e-6)&(hi>z+1e-6)):
                raise AssertionError('A sampled stair-foot disk lacks actual mesh support or penetrates a solid')
    return minimum


def connected_spaces(plan):
    spaces = {identifier for row in plan['storey_room_ids'] for identifier in row}
    graph = {identifier:set() for identifier in spaces|{'outside'}}
    for portal in [item for row in plan['storey_portals'] for item in row]+plan['vertical_portals']:
        linked = portal['rooms']+(['outside'] if portal.get('outside') else [])
        if len(linked)!=2 or any(identifier not in graph for identifier in linked):
            raise AssertionError('Storey portal has invalid room ownership')
        a,b = linked
        graph[a].add(b)
        graph[b].add(a)
    reached,pending = set(),['outside']
    while pending:
        identifier = pending.pop()
        if identifier not in reached:
            reached.add(identifier)
            pending.extend(graph[identifier]-reached)
    if reached != set(graph):
        raise AssertionError('A constructed storey or room lacks access from the entrance')
    return len(spaces)


def approach(plan):
    door = next(item for item in plan['doors'] if set(item['rooms'])=={0,1})
    centre,tangent = np.asarray(door['centre']),np.asarray(door['tangent'])
    normal = np.array([tangent[1],-tangent[0]])
    sides = [centre-normal*.46,centre+normal*.46]
    inside = next(point for point in sides if plan['rooms'][1].buffer(1e-7).covers(shapely.Point(point)))
    outside = next(point for point in sides if np.linalg.norm(point-inside)>.1)
    return np.array([outside,centre,inside])


def landing_poses(points, height):
    sampled = []
    for a,b in zip(points,points[1:]):
        count = max(2,int(np.ceil(np.linalg.norm(b-a)/.10))+1)
        sampled.extend(np.c_[np.linspace(a,b,count),np.full(count,height)])
    return np.asarray(sampled)


def verify(plan, product):
    if plan.get('vertical_status') != 'connected-storeys':
        raise AssertionError('No complete constructive storey plan')
    if plan.get('circulation_status')=='direct-rooms':
        if plan['storeys']!=1 or 'stair' in plan or plan['vertical_portals']:
            raise AssertionError('Direct-room access cannot represent unconnected upper storeys')
        verify_layout(plan,first_mesh(plan))
    else:verify_access(plan,first_mesh(plan))
    connected_spaces(plan)
    closed_parts(product)
    bounds = product['vertices'][:,2].min(),product['vertices'][:,2].max()
    if not np.allclose(bounds,[plan['base_z_m'],plan['body_top_m']],atol=1e-7,rtol=0):
        raise AssertionError('Constructed storeys changed the delivered vertical envelope')
    for index in range(plan['storeys']):
        current = level(plan,index)
        expected = current['footprint']
        if index and 'stair' in plan:
            expected = expected.difference(plan['stair']['opening'])
        floor = occupancy(product,0,current['floor_z_m']-.01,current['floor_z_m']+.01)
        if floor.symmetric_difference(expected).area>1e-6:
            raise AssertionError('Actual storey slab fails to preserve its source shape and declared core opening')
    landing = approach(plan) if 'stair' in plan else None
    columns = Columns(product,np.r_[plan['stair']['path'][:,:2],landing]) if 'stair' in plan else None
    tested,headroom = 0,float('inf')
    if 'stair' in plan:
        stair = plan['stair']
        if stair['riser_m']>MAX_RISER_M+1e-9:
            raise AssertionError('A stair riser exceeds the declared 0.18 m maximum')
        for index in range(plan['storeys']-1):
            poses = stair['path'].copy()
            poses[:,2] += index*stair['rise_m']
            if abs(poses[-1,2]-poses[0,2]-stair['rise_m'])>1e-9 or np.max(np.diff(poses[:,2]))>MAX_RISER_M+1e-9:
                raise AssertionError('Stair contacts skip a storey or a permitted step height')
            headroom = min(headroom,inspect_poses(columns,poses))
            swept=[]
            for a,b in zip(poses,poses[1:]):
                count=max(2,int(np.ceil(np.linalg.norm(b[:2]-a[:2])/.10))+1)
                points=np.linspace(a,b,count)
                points[:,2]=max(a[2],b[2])
                swept.extend(points)
            headroom = min(headroom,inspect_poses(columns,np.asarray(swept),feet=False))
            bottom = landing_poses(np.r_[landing,poses[:1,:2]],poses[0,2])
            top = landing_poses(np.r_[poses[-1:,:2],landing[::-1]],poses[-1,2])
            headroom = min(headroom,inspect_poses(columns,np.r_[bottom,top]))
            tested += len(poses)+len(swept)+len(bottom)+len(top)
    return dict(storeys=plan['storeys'],spaces=len(plan['rooms'])*plan['storeys'],
                constructed_stairs='stair' in plan,stair_connections=max(0,plan['storeys']-1),
                sampled_poses=tested,sampled_min_headroom_m=headroom if np.isfinite(headroom) else None,
                feet_radius_m=.10,torso_radius_m=.30,torso_low_m=.40,body_height_m=1.90,
                native_collision=False,body_height_interval_m=list(bounds),
                source_height_interval_m=[plan['base_z_m'],plan['source_top_m']])


def compare_flights():
    rows=[]
    for rise in (2.889,3.,4.5):
        count=int(np.ceil(rise/MAX_RISER_M/2))
        descriptor=dict(origin=[2.,-3.],direction=[.6,.8],base_z_m=1.6,risers=count,riser_m=rise/(2*count))
        riser = descriptor['riser_m']
        expected=FLIGHT_WIDTH_M*TREAD_M*((count-1)*(PLATE_M+riser/2)-(PLATE_M-FLOOR_M)**2/(2*riser))
        products=[]
        for method in ('profile','tread-parts'):
            timings = []
            for repetition in range(22):
                storage=[],[],[],[]
                elapsed=flight(storage,descriptor,method)
                if repetition:
                    timings.append(elapsed)
            product=dict(vertices=np.asarray(storage[0]),faces=np.asarray(storage[1]),
                         materials=np.asarray(storage[2]),parts=np.asarray(storage[3]))
            volume=closed_parts(product)
            if abs(volume-expected)>1e-8:
                raise AssertionError('A transformed stair flight changed the analytic volume')
            products.append(product)
            rows.append(dict(rise_m=rise,method=method,generation_ms=float(np.median(timings)),
                             p95_generation_ms=float(np.quantile(timings,.95)),repetitions=len(timings),
                             triangles=len(product['faces']),parts=len(np.unique(product['parts'])),volume_m3=volume))
        points=np.array([[2.,-3.]])+np.linspace(.01,(count-1)*TREAD_M-.01,51)[:,None]*[.6,.8]
        a,b=Columns(products[0]).intervals(points),Columns(products[1]).intervals(points)
        first=np.max(a[1],axis=1)
        second=np.max(b[1],axis=1)
        if not np.allclose(first,second,atol=1e-8,rtol=0):
            raise AssertionError('The two stair constructions disagree on actual tread support heights')
    return rows


def controls(plan, product):
    checks = {}
    sealed = mesh(plan,seal_openings=True)
    try:
        inspect_poses(Columns(sealed),plan['stair']['path'])
    except AssertionError as error:
        if 'obstructs' not in str(error):
            raise
        checks['sealed_storey_opening_rejected_by_actual_headroom'] = True
    else:
        raise AssertionError('A sealed inter-storey opening passed the constructive verifier')
    selected = product['materials'] != 5
    missing = {key:value if key=='vertices' else value[selected] for key,value in product.items()}
    try:
        inspect_poses(Columns(missing),plan['stair']['path'])
    except AssertionError as error:
        if 'lacks actual mesh support' not in str(error):
            raise
        checks['missing_stair_treads_rejected_by_actual_support'] = True
    else:
        raise AssertionError('Absent stair flights passed actual contact verification')
    return checks


def storey_scope():
    sample = dict(id='storey-scope',polygon=shapely.box(0,0,14,10),
                  properties={'render_height':9.,'building:levels':2})
    plan,repeat = build(sample),build(sample)
    if plan['storeys']!=2 or plan['storey_room_ids']!=repeat['storey_room_ids'] or plan['storey_portals']!=repeat['storey_portals']:
        raise AssertionError('Delivered storeys or stable room/portal identities changed')
    verify(plan,mesh(plan))
    sample = dict(id='storey-budget',polygon=shapely.box(0,0,14,10),
                  properties={'render_height':195.,'building:levels':65})
    omitted = build(sample)
    if omitted['vertical_status']!='unplanned' or not any('64-storey' in reason for reason in omitted['omissions']):
        raise AssertionError('Constructive storey budget was exceeded or its omission reason was lost')
    return dict(delivered_levels_preserved=True,stable_storey_portals=True,storey_budget_omission=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/vertical-validation.json'))
    args=parser.parse_args()
    rows,omitted=[],[]
    checked_controls=None
    for sample in cases():
        plan=build(sample)
        if plan['vertical_status']!='connected-storeys':
            omitted.append(dict(label=sample['label'],reasons=plan['omissions']))
            continue
        product=mesh(plan)
        result=verify(plan,product)
        rows.append(result|dict(label=sample['label']))
        print(json.dumps(dict(case=sample['label'],**result)),flush=True)
        if sample['label']=='L-wing':
            checked_controls=controls(plan,product)
    if len(rows)!=8 or len(omitted)!=3 or checked_controls is None:
        raise AssertionError('Constructive storey coverage or its sealed-slab control is missing')
    checked_controls.update(storey_scope())
    result=dict(**revision(),cases=rows,omissions=omitted,controls=checked_controls,flight_comparison=compare_flights(),
                scope='Closed components, actual support disks and sampled lifted-step torso; native continuous player collision unproved')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(status='pass',buildings=len(rows),storeys=sum(row['storeys'] for row in rows),controls=checked_controls)))


if __name__=='__main__':
    main()
