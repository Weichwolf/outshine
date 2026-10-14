import json,sys
from pathlib import Path
from samples import read_site
from sfcgal_partition_probe import native_plan
from outshine_ref.buildings.modelling import prepare_hip
from verify_scope_library import costs
from verify_skeleton_plan import verify
from evidence import revision
samples=[]
for site in ('Vienna','Tokyo','Rosenheim'):
 samples.extend(sorted(read_site(site)[0],key=lambda s:len(s['polygon'].exterior.coords),reverse=True)[:16])
def python_first(polygon):
 try:return prepare_hip(polygon,'skeleton')
 except ValueError:return native_plan(polygon)
for sample in samples:verify(python_first(sample['polygon']));verify(prepare_hip(sample['polygon']))
results=costs(dict(python_first=lambda:[python_first(s['polygon']) for s in samples],native_first=lambda:[prepare_hip(s['polygon']) for s in samples]))
p=revision()|dict(status='pass',sources=[dict(source_id=s['id'],sha256=s['source_sha256']) for s in samples],measurements=results,scope='Identical full set of 48 sources, each independently validated; 21 alternating warm batch preparations; not per-frame or rendering.')
Path('build/experiments/buildings/hip-research/skeleton-batch-final.json').write_text(json.dumps(p,indent=2)+'\n')
print(json.dumps(results))
