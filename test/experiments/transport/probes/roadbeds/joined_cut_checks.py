"""Check joined ownership, logical port subdivision and retained unselected contacts."""

import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_cut_setbacks import CutSetbackRejected,setback_shared_section,setback_shared_sections
from transport_joined_cut_setbacks import partition_axis_ports
from transport_cut_setback_experiment import verify_setbacks,verify_loop_setbacks,verify_sequential_precision,verify_contact_progress


def verify_joined_cut():
    area=shapely.box(-10,-4,10,4).difference(shapely.box(-2,-1,6,1))
    core=area.intersection(shapely.box(0,-10,20,10));road=area.difference(core)
    sections=[shapely.LineString([[0,-4],[0,-1]]),shapely.LineString([[0,1],[0,4]])]
    axes=np.array([[-4.,-2.5],[-4.,2.5]])
    for angle,offset in ((0.,[0.,0.]),(.7,[10000.,-7000.]),(2.1,[-300.,50.])):
        matrix=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform=lambda xy:xy@matrix.T+offset
        a,b=(shapely.transform(g,transform) for g in (core,road))
        old=[shapely.transform(g,transform) for g in sections];point=transform(np.array([-4.,-2.5]));tangent=matrix@np.array([1.,0.])
        try:setback_shared_section(a,b,old[0],point,tangent)
        except CutSetbackRejected as error:assert str(error)=='shared_section_mismatch'
        else:raise AssertionError('an unselected source port was consumed')
        c,r,line=setback_shared_sections(a,b,old,point,tangent)
        assert np.allclose([c.area,r.area],[96.,48.],atol=1e-7,rtol=0)
        assert c.union(r).symmetric_difference(shapely.transform(area,transform)).area<1e-7
        assert c.intersection(r).area==0 and len(c.union(r).interiors)==1
        assert line.hausdorff_distance(c.boundary.intersection(r.boundary))<1e-7
        ports=partition_axis_ports(line,transform(axes))
        assert abs(sum(p.length for p in ports)-line.length)<1e-10
        assert ports[0].intersection(ports[1]).geom_type=='Point'
        for p,axis in zip(ports,transform(axes)):assert p.distance(shapely.Point(axis))<1e-10
        reversed_ports=partition_axis_ports(line,transform(axes[::-1]))
        assert all(p.equals_exact(q,1e-10) for p,q in zip(ports,reversed_ports[::-1]))
    line=shapely.LineString([[0,0],[0,8]])
    for points in ([[0,2],[0,2]],[[0,2],[0,10]],[[1,2],[0,6]]):
        try:partition_axis_ports(line,points)
        except CutSetbackRejected:pass
        else:raise AssertionError('ambiguous or off-line source axes accepted')
    return dict(joined_transfer_conserves_area=True,global_traffic_island_retained=True,
                distinct_logical_ports=True,input_order_independent=True,rigid_transforms=True,
                unselected_contact_preserved=True,ambiguous_source_axes_rejected=True)


if __name__=='__main__':print(json.dumps(verify_setbacks()|verify_loop_setbacks()|verify_sequential_precision()|verify_contact_progress()|verify_joined_cut()))
