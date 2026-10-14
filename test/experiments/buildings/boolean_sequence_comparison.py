"""Equal-quality room/door expressions: union cutters first or subtract sequentially."""

import json
from pathlib import Path
import resource
import time

import numpy as np
import shapely

from boolean_sequence import combine
from boolean_sequence_reference import reference,section,generated_section
from verify_boolean_sequence import source,verify
from verify_boolean_surfaces import Prism
from evidence import revision


def leaf_areas(node):
    mesh=node.product.overlay.product.mesh;triangles=mesh['vertices'][mesh['faces']]
    area=np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1)/2
    result={}
    for preferred,value in zip(node.preferred,area,strict=True):
        key=(preferred.source,preferred.face);result[key]=result.get(key,0.)+float(value)
    return result


def run():
    prisms=dict(outer=Prism(shapely.box(0,0,12,8),0.,6.),room=Prism(shapely.box(2,2,10,6),.2,7.),
        door=Prism(shapely.box(5,-1,7,2.5),.2,3.2))
    declared={name:source(name,prism,index%2) for index,(name,prism) in enumerate(prisms.items())}
    target=('subtract','outer',('union','room','door'));expected=reference(prisms,target)
    runs=[];areas=None
    for repeat in range(3):
        strategies=('union-first','subtract-sequential') if repeat%2==0 else ('subtract-sequential','union-first')
        for strategy in strategies:
            start=time.perf_counter()
            record=dict(strategy=strategy,repeat=repeat)
            try:
                if strategy=='union-first':
                    cutter=combine('cutters',declared['room'],declared['door'])
                    final=combine('final',declared['outer'],cutter.operand,'subtract')
                else:
                    first=combine('room-cut',declared['outer'],declared['room'],'subtract')
                    final=combine('final',first.operand,declared['door'],'subtract')
                elapsed=(time.perf_counter()-start)*1000;verify(final,expected)
                values=leaf_areas(final)
                if areas is None:areas=values
                assert values.keys()==areas.keys()
                for key,value in values.items():np.testing.assert_allclose(value,areas[key],atol=1e-7,rtol=1e-9)
                for height in (.3,1.,2.):
                    actual=generated_section(final,height)
                    if actual.symmetric_difference(section(prisms,target,height)).area>1e-7:
                        raise ValueError('Compared strategy changed generated room section')
                record.update(status='pass',pipeline_ms=elapsed,triangles=len(final.origins),logical_faces=len(final.product.regions.faces),
                    output_array_bytes=sum(value.nbytes for value in final.product.overlay.product.mesh.values()),
                    output_repair=final.product.overlay.output_repair)
            except Exception as error:record.update(status='rejected',reason=str(error))
            runs.append(record)
    return runs


if __name__=='__main__':
    rows=run();report=revision()|dict(status='pass' if all(row['status']=='pass' for row in rows) else 'comparison-rejected',runs=rows,
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        measurement_scope='pipeline_ms covers two complete geometry/provenance/Shape operations, excluding independent verification; peak RSS covers the combined process, not either strategy individually.',
        scope='One fixed room/door workload, six alternating complete two-step pipelines; equal volume/surface/source UV/material leaf areas/generated slices, current tags correct within each expression. No general runtime winner.')
    Path('build/experiments/buildings/boolean-sequence-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(rows),flush=True)
    raise SystemExit(0 if report['status']=='pass' else 1)
