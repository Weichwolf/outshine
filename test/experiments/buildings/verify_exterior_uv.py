"""Independent affine UV continuation and partition coverage on controls and real eaves."""

import json
from pathlib import Path

import numpy as np
import shapely
from shapely import affinity

from compare_roof_oracle import cases
from evidence import revision
from exterior_uv_probe import partition, values
from hip_overhang_probe import extend
from outshine_ref.buildings.modelling import prepare_hip
from verify_modelling_roof_shed import source


def verify(row, even=False,value=3.,mode='byHeight'):
    shape=source(row['polygon']);mesh=shape.emit();plan=prepare_hip(row['polygon'])
    xy=mesh['vertices'][:,[0,2]]*[1.,-1.]-plan.origin
    corners=xy[mesh['faces']]
    properties=np.concatenate([mesh['uv'+str(c) if c else 'uv'][mesh['faces']] for c in range(10)],axis=2)
    extension=extend(plan,value,mode,.5,even)
    rings=(plan.footprint.exterior,*plan.footprint.interiors)
    regions=checked=0;maximum_error=0.;invalid=[]
    for number,facet in enumerate(plan.facets):
        domain=affinity.translate(extension.facets[number],-plan.origin[0],-plan.origin[1])
        if not domain.is_valid:
            invalid.append(number);continue
        exterior=domain.difference(plan.footprint)
        if exterior.is_empty:continue
        ring,edge=facet.boundary;coords=np.asarray(rings[ring].coords)
        charts=partition(corners,coords[edge],coords[edge+1],exterior)
        coverage=shapely.union_all([c.polygon for c in charts])
        tolerance=max(1e-7,exterior.area*1e-10)
        assert coverage.symmetric_difference(exterior).area<=tolerance
        assert sum(c.polygon.area for c in charts)-coverage.area<=tolerance
        for chart in charts:
            points=np.vstack((np.asarray(chart.polygon.exterior.coords)[:-1],chart.polygon.centroid.coords))
            actual=values(corners,properties,chart.triangle,points)
            # Independent Cartesian affine fit, not the partition's barycentric solve.
            fit=np.linalg.solve(np.c_[corners[chart.triangle],np.ones(3)],properties[chart.triangle])
            expected=np.c_[points,np.ones(len(points))]@fit
            error=float(abs(actual-expected).max());assert error<=1e-7
            maximum_error=max(maximum_error,error);checked+=actual.size
        regions+=len(charts)
    return dict(id=row['id'],even=even,value=value,mode=mode,charts=regions,uv_values_checked=checked,
                maximum_affine_error=maximum_error,invalid_geometry_facets=invalid)


if __name__=='__main__':
    results=[]
    for row in cases():
        for even in (False,True):
            for value,mode in ((3.,'byHeight'),(25.,'byAngle')):
                try:results.append(verify(row,even,value,mode))
                except Exception as error:raise AssertionError((row['id'],even,mode)) from error
    record=revision()|dict(status='pass',results=results,
        scope='Exterior UV chart continuation on each individually valid positive eave-ray facet. Invalid geometric facets retained, not a complete positive-overhang or SDK UV proof.')
    Path('build/experiments/buildings/exterior-uv-verification.json').write_text(json.dumps(record,indent=2)+'\n')
    print('cases',len(results),'charts',sum(r['charts'] for r in results),'UV values',sum(r['uv_values_checked'] for r in results))
