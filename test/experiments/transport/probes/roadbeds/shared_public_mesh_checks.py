"""Only triangulated public neighbor area may support a shared-port vehicle hull."""

from pathlib import Path
import sys
from types import SimpleNamespace

import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_coupled_roadbed_experiment import publish_coupled_roadbeds


def verify():
    left=shapely.box(-4,-2,1,2);right=shapely.box(1,-2,4,2)
    line=shapely.LineString([[1,-2],[1,2]])
    trial=SimpleNamespace(junctions={0:[dict(section=7,corridor=1,line=line)]},corridors={1:[(0,7)]})
    def products(context,public):
        result={}
        for owner,physical,floor in ((0,left,left),(1,right,public)):
            result[owner]=dict(base=floor,core=physical,planned=floor,physical=physical,
                               quantum=2.**-30,curves=[],poses=[])
        result[0]['shared_public_context']=context
        return result
    context=shapely.box(1,-.5,2,.5)
    _,_,_,surfaces=publish_coupled_roadbeds(products(context,right),trial,.025)
    assert shapely.from_wkb(surfaces[0]['public_surface_wkb']).covers(context)
    for invalid,public in ((shapely.box(4,-.5,5,.5),right),(context,shapely.box(1,-2,4,-1))):
        try:publish_coupled_roadbeds(products(invalid,public),trial,.025)
        except AssertionError as error:assert str(error)=='shared public context is absent from native public mesh'
        else:raise AssertionError('unproved neighbor surface accepted')
    publish_coupled_roadbeds(products(shapely.Polygon(),right),trial,.025)
    print('shared public mesh: PASS; absent/private neighbor area rejected, empty context retained')


if __name__=='__main__':verify()
