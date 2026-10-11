"""A bounded single-level vector plan with explicit source usage and local repairs."""

from collections import Counter,defaultdict
import hashlib
import json
import time

import numpy as np
import shapely

from infrastructure_network import source_roads,source_identity as identity
from infrastructure_network_graph import axes_of,traffic_of


def identifier(road):
    geometry,tags = identity(road)
    return hashlib.sha256(geometry+json.dumps(tags).encode()).hexdigest()[:24]


def bind_sources(roads,graphs,tolerance):
    for graph in graphs.values():
        sources = graph['source_indices']
        lines = np.array([roads[i]['line'] for i in sources],dtype=object)
        midpoints = shapely.line_interpolate_point(graph['lines'],.5,normalized=True)
        pairs = shapely.STRtree(lines).query(midpoints,predicate='dwithin',distance=tolerance)
        order = np.lexsort((pairs[1],pairs[0]))
        pairs = pairs[:,order]
        counts = np.bincount(pairs[0],minlength=len(graph['lines']))
        if np.any(counts==0):
            raise ValueError('noded edge without an owning source')
        graph['owner_offsets'] = np.r_[0,np.cumsum(counts)]
        graph['owner_sources'] = sources[pairs[1]]
        first = shapely.line_interpolate_point(graph['lines'],.45,normalized=True)[pairs[0]]
        second = shapely.line_interpolate_point(graph['lines'],.55,normalized=True)[pairs[0]]
        owners = lines[pairs[1]]
        first_station = shapely.line_locate_point(owners,first)
        second_station = shapely.line_locate_point(owners,second)
        delta = second_station-first_station
        length = shapely.length(owners)
        closed = shapely.is_closed(owners)
        delta[closed] = (delta[closed]+length[closed]*.5)%length[closed]-length[closed]*.5
        uncertainty = 32*np.spacing(np.maximum(np.abs(first_station),np.abs(second_station)))
        graph['owner_directions'] = np.where(np.abs(delta)>uncertainty,np.sign(delta),0).astype(np.int8)


def repair_ends(roads, precision, budget):
    lines = np.array([r['line'] for r in roads],dtype=object)
    tree = shapely.STRtree(lines)
    endpoints = np.array([shapely.get_coordinates(line)[[0,-1]] for line in lines]).reshape(-1,2)
    additions,changes = defaultdict(list),{}
    pairs_seen,limited = 0,False
    for start in range(0,len(endpoints),budget['endpointBatch']):
        positions = endpoints[start:start+budget['endpointBatch']]
        pairs = tree.query(shapely.points(positions),predicate='dwithin',
                           distance=budget['endpointToleranceM'])
        pairs_seen += pairs.shape[1]
        if pairs_seen > budget['maximumEndpointPairs']:
            limited = True
            break
        candidates,connected = defaultdict(list),set()
        for point,target in pairs.T:
            endpoint = start+int(point)
            source = endpoint//2
            if source==target or traffic_of(roads[source])!=traffic_of(roads[target]):
                continue
            p = shapely.Point(endpoints[endpoint])
            station = lines[target].project(p)
            q = lines[target].interpolate(station)
            distance = p.distance(q)
            if distance<=precision*.01:
                connected.add(endpoint)
            elif not (min(station,lines[target].length-station)<precision and target>source):
                candidates[endpoint].append((distance,int(target),station,np.array(q.coords[0])))
        for endpoint,options in candidates.items():
            if endpoint in connected:
                continue
            distance,target,station,point = min(options,key=lambda v:(v[0],v[1]))
            changes[endpoint] = (point,distance)
            additions[target].append((station,point))
    result = []
    for index,road in enumerate(roads):
        xy = shapely.get_coordinates(road['line'])
        stations = np.r_[0,np.cumsum(np.linalg.norm(np.diff(xy,axis=0),axis=1))]
        points = [(s,p) for s,p in zip(stations,xy)]
        for end,station in ((0,0.),(1,float(stations[-1]))):
            if index*2+end in changes:
                points[0 if end==0 else -1] = (station,changes[index*2+end][0])
        points += additions[index]
        ordered = np.array([p for _,p in sorted(points,key=lambda v:v[0])])
        ordered = np.round(ordered/precision)*precision
        ordered = ordered[np.r_[True,np.any(np.diff(ordered,axis=0)!=0,axis=1)]]
        if len(ordered)<2:
            result.append(dict(road,rejection='collapsed_axis',merged_position=ordered[0].tolist()))
        else:
            result.append(dict(road,line=shapely.LineString(ordered)))
    return result,dict(adjusted_endpoints=len(changes),candidate_pairs=pairs_seen,
                       repair_budget_exhausted=limited,
                       maximum_endpoint_adjustment_m=max((d for _,d in changes.values()),default=0.))


def plan(transport, recipes, widths, precision=.001):
    began = time.perf_counter()
    normalized,_ = source_roads(transport,recipes,widths)
    eligible,rejected = [],{}
    for road in normalized:
        key = identity(road)
        if road['tier']!=(0,'ground'):
            rejected[key] = 'deferred_level'
        elif road['properties'].get('indoor') in ('yes',1,True):
            rejected[key] = 'deferred_indoor_recipe'
        else:
            eligible.append(dict(road,source_key=key,source_id=identifier(road)))
    budget = recipes['flatPlan']
    eligible.sort(key=lambda road:road['line'].distance(shapely.Point(0,0)))
    for road in eligible[budget['maximumAxes']:]:
        rejected[road['source_key']] = 'axis_budget_exhausted'
    eligible = eligible[:budget['maximumAxes']]
    corrected,repairs = repair_ends(eligible,precision,budget) if eligible else ([],{})
    accepted,junctions = [],[]
    for road in corrected:
        if 'rejection' in road:
            junctions.append(road)
        else:
            accepted.append(road)
    graphs,stats,_ = axes_of(accepted,precision)
    bind_sources(accepted,graphs,precision*1e-4)
    merged = {}
    for road in junctions:
        graph = graphs.get((traffic_of(road),*road['tier']))
        point = np.array(road['merged_position'])
        if graph is not None and np.any(np.linalg.norm(graph['vertices']-point,axis=1)<precision*.01):
            merged[road['source_key']] = road
        else:
            rejected[road['source_key']] = 'collapsed_axis_without_shared_node'
    used = {road['source_key'] for road in accepted}
    normalization = {identity(r) for r in normalized}
    represented,usage = set(),[]
    for index,source in enumerate(transport):
        key = identity(source)
        if key in used or key in merged:
            status = ('duplicate_represented' if key in represented else
                      'represented_by_junction' if key in merged else 'used')
            represented.add(key)
            reason = ''
        else:
            status = 'not_used'
            reason = rejected.get(key,'route_or_missing_recipe' if key not in normalization else 'unresolved')
        usage.append(dict(source=index,id=identifier(source),status=status,reason=reason,
                          length_m=source['line'].length))
    counts = Counter(u['status'] for u in usage)
    reasons = Counter(u['reason'] for u in usage if u['status']=='not_used')
    lengths = Counter()
    for u in usage:
        lengths[u['status']] += u['length_m']
    elapsed_ms = (time.perf_counter()-began)*1000
    validation_ms = sum(g['source_coverage_audit_ms'] for g in stats)
    return accepted,graphs,dict(scope='Single horizontal level with source direction; traffic rules, curves and heights remain open.',
           source_axes=len(transport),planned_axes=len(accepted),states=dict(counts),reasons=dict(reasons),
           unresolved_owner_directions=sum(int(np.count_nonzero(g['owner_directions']==0)) for g in graphs.values()),
           length_m=dict(lengths),repairs=repairs,graphs=stats,usage=usage,
           merged_junction_sources=[dict(id=r['source_id'],position_m=r['merged_position'],
                                    width_m=r['width'],properties=r['properties']) for r in merged.values()],
           source_ids=[road['source_id'] for road in accepted],
           elapsed_ms=elapsed_ms,source_coverage_audit_ms=validation_ms,
           planning_and_source_binding_ms=elapsed_ms-validation_ms,
           budget=budget,hard_wall_time_bound=False)


def examples():
    def road(xy,layer=0,width=4.,**properties):
        return dict(line=shapely.LineString(xy),width=width,properties={'class':'minor','layer':layer,**properties})
    cases = {
        'straight':[road([(0,0),(20,0)])],
        'T':[road([(-10,0),(10,0)]),road([(0,0),(0,10)])],
        'X':[road([(-10,0),(10,0)]),road([(0,-10),(0,10)])],
        'bend':[road([(0,0),(10,0),(10,10)])],
        'cul_de_sac':[road([(0,0),(10,0)]),road([(10,0),(20,5)])],
        'short_link':[road([(-10,0),(0,0)]),road([(0,0),(.1,0)]),road([(.1,0),(10,2)])],
        'small_gap':[road([(-10,0),(10,0)]),road([(0,.2),(0,10)])],
        'short_T':[road([(-10,0),(10,0)]),road([(0,0),(0,5)])],
        'split_continuation':[road([(-20,0),(-2,0)]),road([(-2,0),(20,0)]),road([(0,0),(0,20)])],
        'bent_short_approach':[road([(-20,0),(20,0)]),road([(0,0),(2,3)]),road([(2,3),(7,10)])],
        'acute_merge':[road([(-30,0),(30,0)]),road([(0,0),(30,3)])],
        'courtyard_loop':[road([(0,0),(20,0),(20,20),(0,20),(0,0)]),
                          road([(-20,0),(0,0)]),road([(20,20),(40,20)])],
        'island':[road([(-10,0),(0,0)]),road([(20,0),(30,0)])]}
    angle = np.deg2rad(20)
    rotation = np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
    cases['inclined_T'] = [dict(r,line=shapely.transform(np.array([r['line']],dtype=object),
                            lambda xy:xy@rotation.T)[0]) for r in cases['T']]
    return cases


def verify(recipes):
    cases = examples()
    reports = []
    for name,roads in cases.items():
        accepted,graphs,report = plan(roads,recipes,{})
        assert report['states'].get('not_used',0)==0,(name,report['states'],report['reasons'])
        expected = 2 if name=='island' else 1
        assert sum(g['components'] for g in report['graphs'])==expected,(name,report)
        reports.append(dict(case=name,components=expected,elapsed_ms=report['elapsed_ms']))
    constrained = dict(recipes,flatPlan=dict(recipes['flatPlan'],maximumAxes=1))
    other = dict(line=shapely.LineString([(0,0),(0,20)]),width=4.,properties={'class':'minor','layer':1})
    _,_,limited = plan(cases['X']+[other],constrained,{})
    assert limited['reasons']=={'axis_budget_exhausted':1,'deferred_level':1}
    return reports
