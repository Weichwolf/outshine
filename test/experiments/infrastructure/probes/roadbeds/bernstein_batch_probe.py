import ast,json,subprocess,sys,time
from functools import cache
from math import comb
from pathlib import Path
import numpy as np
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_spline_routes import curve_bounds

@cache
def product_terms(a,b):
    rows=[]
    for i in range(a+1):
        j=np.arange(b+1)
        k=i+j
        weights=np.array([comb(a,i)*comb(b,int(n))/comb(a+b,int(i+n)) for n in j])
        rows.append((i,j,k,weights))
    return rows

original_source=subprocess.check_output(['git','show','5c76be01c^:test/experiments/infrastructure_spline_routes.py'],text=True)
definition=next(n for n in ast.parse(original_source).body if isinstance(n,ast.FunctionDef) and n.name=='curve_bounds')
source=ast.get_source_segment(original_source,definition)
namespace=dict(curve_bounds.__globals__)
exec(compile(source,'baseline-curve-bounds','exec'),namespace)
curve_bounds=namespace['curve_bounds']
start=source.index('    for i in range(a+1):')
end=source.index('    curvature =',start)
replacement='''    for i,j,k,weights in product_terms(a,b):
        cross[:,i:i+b+1] += weights*(velocity[:,i,0,None]*acceleration[:,:,1]-velocity[:,i,1,None]*acceleration[:,:,0])
'''
namespace=dict(curve_bounds.__globals__,product_terms=product_terms)
exec(source[:start]+replacement+source[end:],namespace)
variant=namespace['curve_bounds']
rng=np.random.default_rng(2343)
rows=[]
for degree in (3,5,7):
 for count in (1,8,48,128):
  controls=np.cumsum(rng.normal(size=(count,degree+1,2)),axis=1)
  controls[0]=0
  expected=curve_bounds(controls);actual=variant(controls)
  for a,b in zip(expected,actual):np.testing.assert_array_equal(a,b)
  times=[]
  for function in (curve_bounds,variant):
   samples=[]
   for _ in range(5):
    began=time.perf_counter()
    for _ in range(200):function(controls)
    samples.append((time.perf_counter()-began)/200*1e6)
   times.append(float(np.median(samples)))
  rows.append(dict(degree=degree,segments=count,old_us=times[0],batch_us=times[1],speedup=times[0]/times[1]))
print(json.dumps(rows,indent=2))
Path('build/experiments/infrastructure/roadbeds/bernstein_batch_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
