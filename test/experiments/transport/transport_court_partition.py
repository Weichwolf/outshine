"""Assign added court area to adjacent modules without moving source cut planes."""

import numpy as np
import shapely
from shapely.ops import linemerge

from transport_surface_regions import canonical_grid_region,polygonal_region
from transport_turn_roadbeds import geometry_precision,preserves_coverage,coordinate_roundoff


def port_cell(port,ports,extent,quantum):
    centre=np.mean(np.array(port['line'].coords)[[0,-1]],axis=0)
    cell=None
    for other in ports:
        if other['owner']!=port['owner'] or other['section']==port['section']:continue
        rival=np.mean(np.array(other['line'].coords)[[0,-1]],axis=0)
        normal=centre-rival;distance=np.linalg.norm(normal)
        assert distance>quantum,'coincident ports need one shared section'
        normal/=distance;tangent=np.array([-normal[1],normal[0]])
        midpoint=(centre+rival)*.5;a=midpoint-tangent*extent;b=midpoint+tangent*extent
        half=shapely.set_precision(shapely.Polygon([a,b,b+normal*extent,a+normal*extent]),quantum)
        cell=half if cell is None else cell.intersection(half)
    return cell


def partition_court(parts,core,addition,ports):
    quantum=geometry_precision(shapely.union_all([*parts.values(),addition]))
    original={k:canonical_grid_region(g,quantum) for k,g in parts.items()}
    result=dict(original)
    remaining=canonical_grid_region(addition.difference(shapely.union_all(list(original.values()))),quantum)
    bounds=shapely.union_all([*original.values(),remaining]).bounds
    extent=max(bounds[2]-bounds[0],bounds[3]-bounds[1])*4
    for port in sorted(ports,key=lambda p:p['section']):
        line=shapely.set_precision(port['line'],quantum)
        xy=np.array(line.coords);direction=xy[-1]-xy[0]
        factor=2.**np.ceil(np.log2(max(1.,extent/np.linalg.norm(direction))))
        normal=np.array([-direction[1],direction[0]])
        if normal@port['away']<0:normal=-normal
        a=xy[0]-direction*factor;b=xy[-1]+direction*factor
        outside=shapely.Polygon([a,b,b+normal*factor,a+normal*factor])
        outside=shapely.set_precision(outside,quantum)
        cell=port_cell(port,ports,extent,quantum)
        if cell is not None:outside=outside.intersection(cell)
        piece=remaining.intersection(outside)
        result[port['owner']]=canonical_grid_region(result[port['owner']].union(piece),quantum)
        remaining=polygonal_region(remaining.difference(outside))
    result[core]=canonical_grid_region(result[core].union(remaining),quantum)
    lines={}
    for port in ports:
        shared=result[core].boundary.intersection(result[port['owner']].boundary)
        endpoints=np.array(port['line'].coords)[[0,-1]];delta=endpoints[1]-endpoints[0]
        factor=2.**np.ceil(np.log2(max(1.,extent/np.linalg.norm(delta))))
        axis=shapely.LineString([endpoints[0]-delta*factor,endpoints[1]+delta*factor])
        segments=[]
        for line in shapely.get_parts(shared):
            if line.geom_type!='LineString':continue
            points=shapely.get_coordinates(line)
            for a,b in zip(points[:-1],points[1:]):
                if max(shapely.Point(a).distance(axis),shapely.Point(b).distance(axis))<=coordinate_roundoff(original[core]):
                    segments.append(shapely.LineString([a,b]))
        shared=shapely.union_all(segments)
        if shared.geom_type=='MultiLineString':shared=linemerge(shared)
        assert shared.geom_type=='LineString','court creates a disconnected port'
        assert shared.buffer(1e-7).covers(port['line']),'court removes the original port'
        lines[port['section']]=shared
    expected=shapely.union_all([*original.values(),addition]);actual=shapely.union_all(list(result.values()))
    tolerance=max(1e-8,expected.area*1e-12)
    assert actual.symmetric_difference(expected).area<=tolerance
    assert abs(sum(g.area for g in result.values())-actual.area)<=tolerance
    assert all(preserves_coverage(original[k],g) for k,g in result.items())
    return result,lines,dict(owners=len(result),added_area_m2=actual.difference(shapely.union_all(list(original.values()))).area,
                            transferred_area_m2=sum(original[k].difference(g).area for k,g in result.items()))


def verify():
    parts={0:shapely.box(-2,-2,2,2),1:shapely.box(2,-1,6,1)}
    line=shapely.LineString([[2,-1],[2,1]])
    ports=[dict(section=7,owner=1,line=line,away=np.array([1.,0.]))]
    result,lines,_=partition_court(parts,0,shapely.box(1,-1.5,4,1.5).difference(shapely.union_all(list(parts.values()))),ports)
    assert abs(lines[7].length-3)<1e-10 and lines[7].distance(shapely.Point(2,0))==0
    assert result[1].boundary.buffer(1e-7).covers(shapely.LineString([[6,-1],[6,1]]))
    parts[2]=shapely.box(-1,2,1,6)
    ports.append(dict(section=8,owner=2,line=shapely.LineString([[-1,2],[1,2]]),away=np.array([0.,1.])))
    addition=shapely.union_all([shapely.box(1,-1.5,4,1.5),shapely.box(-1.5,1,1.5,4)])
    addition=addition.difference(shapely.union_all(list(parts.values())))
    result,lines,audit=partition_court(parts,0,addition,ports)
    reverse,reversed_lines,_=partition_court(parts,0,addition,ports[::-1])
    assert all(result[k].equals(reverse[k]) for k in result)
    assert all(lines[s].equals(reversed_lines[s]) and abs(lines[s].length-3)<1e-10 for s in lines)
    assert audit['transferred_area_m2']==0
    shared_parts={0:parts[0],1:parts[1].union(parts[2])}
    shared_ports=[dict(p,owner=1) for p in ports]
    joint,joint_lines,_=partition_court(shared_parts,0,addition,shared_ports)
    swapped,swapped_lines,_=partition_court(shared_parts,0,addition,shared_ports[::-1])
    assert all(joint[k].equals(swapped[k]) for k in joint)
    assert all(joint_lines[k].equals(swapped_lines[k]) for k in joint_lines)
    for port in shared_ports:
        cell=port_cell(port,shared_ports,100.,1e-10)
        centre=np.mean(np.array(port['line'].coords)[[0,-1]],axis=0)
        rival=next(p for p in shared_ports if p['section']!=port['section'])
        assert cell.covers(shapely.Point(centre))
        assert not cell.covers(shapely.Point(np.mean(np.array(rival['line'].coords)[[0,-1]],axis=0)))
    rotation=np.array([[0.,-1.],[1.,0.]]);offset=np.array([1e6,-7e5])
    moved=lambda g:shapely.transform(g,lambda xy:xy@rotation+offset)
    shifted=[dict(p,line=moved(p['line']),away=p['away']@rotation) for p in ports]
    turned,new_lines,_=partition_court({k:moved(g) for k,g in parts.items()},0,moved(addition),shifted)
    assert all(turned[k].equals(moved(g)) for k,g in result.items())
    assert all(new_lines[s].equals(moved(g)) for s,g in lines.items())
    return dict(original_area_preserved=True,added_area_owned_once=True,port_plane_unchanged=True,
                shared_port_extended=True,external_port_unchanged=True,multiple_owners=True,
                port_order_independent=True,rigid_transform=True,same_owner_ports_separated=True)


if __name__=='__main__':print(verify())
