"""Check nested floors against actual slabs and stair clearance, including unsafe requests."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from building_setbacks import apply
from building_vertical import build,mesh
from evidence import revision
from verify_vertical import verify


def controls():
    cases=[('rectangle',shapely.box(0,0,18,14)),
        ('wing',shapely.Polygon([(0,0),(22,0),(22,10),(12,10),(12,18),(0,18)])),
        ('courtyard',shapely.Polygon([(0,0),(26,0),(26,22),(0,22)],holes=[[(9,7),(9,15),(17,15),(17,7)]]))]
    rows=[]
    for name,outline in cases:
        base=build(dict(id=name,polygon=outline,properties={'render_height':9.,'building:levels':3}))
        result=apply(base,[0.,.8,1.6]);product=mesh(result)
        assert result['storey_plans'][0]['footprint'].equals(outline)
        previous=outline
        for current in result['storey_plans']:
            assert current['footprint'].difference(previous).area<1e-7
            assert current['rooms'][1].symmetric_difference(base['rooms'][1]).area<1e-6
            assert current['rooms'][0].symmetric_difference(base['rooms'][0]).area<1e-6
            assert len(current['rooms'])==len(base['rooms'])
            previous=current['footprint']
        rows.append(dict(case=name,decisions=result['setback_decisions'],triangles=len(product['faces']),**verify(result,product)))
        failed=apply(base,[0.,100.,200.])
        assert all(row['reason'] for row in failed['setback_decisions'][1:])
        assert all(row['footprint'].equals(outline) for row in failed['storey_plans'])
        np.testing.assert_array_equal(mesh(failed)['faces'],mesh(base)['faces'])
        if name=='rectangle':
            assert result['storey_plans'][-1]['footprint'].area<outline.area*.8
            try:verify(result,mesh(base))
            except AssertionError:pass
            else:raise AssertionError('Unmodified upper slabs escaped setback verification')
    for values in ([0.,-1.,1.],[0.,2.,1.],[1.,1.,1.],[0.,1.],[0.,float('nan'),2.]):
        try:apply(base,values)
        except ValueError:pass
        else:raise AssertionError('Invalid setback request accepted')
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/setback-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),
        limits='Generic room programme; sampled stair poses, no continuous native collision; terrace access and guards absent')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',cases=len(record['cases']))))
