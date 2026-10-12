"""Check direct lane-loop geometry and eligibility of shared pedestrian crossings."""

from pathlib import Path
import sys
import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from rounded_lane_loop_probe import rounded_polyline
from transport_spline_routes import curve_bounds,continuous_spline_joins
from transport_junction_surfaces import public_pedestrian_surface


def verify():
    points=np.array([[0.,0.],[20,0],[20,20],[0,20],[0,3]])
    for source in (points,points[::-1]):
        controls=rounded_polyline(source,4.5,.5)
        assert continuous_spline_joins(controls) and curve_bounds(controls)[2].max()<.25
        assert np.allclose(controls[0,0],source[0]) and np.allclose(controls[-1,-1],source[-1])
        rotation=np.array([[0.,-1.],[1.,0.]]);offset=np.array([1000.,-700.])
        moved=rounded_polyline(source@rotation+offset,4.5,.5)
        assert np.allclose(moved,controls@rotation+offset,rtol=0,atol=1e-10)
    lines=shapely.linestrings(np.array([[[0.,i*4],[10.,i*4]] for i in range(6)]))
    graph=dict(lines=lines,half_widths=np.ones(6),travel_public=np.array([[4,4],[12,12],[1,1],[0,0],[4,4],[4,4]],dtype=np.uint8),
               travel_restricted=np.array([[0,0],[0,0],[0,0],[4,4],[1,1],[4,4]],dtype=np.uint8))
    surface,ids=public_pedestrian_surface(shapely.box(-2,-2,12,24),graph,shapely.STRtree(lines))
    assert ids.tolist()==[0] and surface.covers(lines[0])
    return dict(clockwise_and_counterclockwise_G2=True,conservative_radius=True,rigid_transform=True,
                public_walk_crossings_only=True,private_and_rail_preserved=True)


if __name__=='__main__':print(verify())
