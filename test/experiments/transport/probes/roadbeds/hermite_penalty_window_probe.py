from pathlib import Path
import inspect,json,sys,time

sys.path.insert(0,'test/experiments/transport')
import transport_hermite_route_fitting as fitting
from transport_roadbed_window_experiment import solve_window,trial_key

root=Path('build/experiments/transport')
base=json.loads((root/'roadbeds/windows-437a9399f/window-roadbeds.json').read_text())
place=next(p for p in base['places'] if p['place']=='ZuerichHauptbahnhof')
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
source=inspect.getsource(fitting.fit_hermite_route)
assert source.count('bends = 4*')==1
source=source.replace('bends = 4*',"bends = (16 if np.dot(entry['heading'],exit['heading'])<-.95 else 4)*")
namespace=dict(fitting.__dict__)
exec(compile(source,str(Path(__file__).resolve()),'exec'),namespace)
fitting.fit_hermite_route=namespace['fit_hermite_route']
output=root/'roadbeds/hairpin-penalty-window-probe'
output.mkdir(parents=True,exist_ok=True)
required={trial_key(t) for t in place['trials'] if t['accepted']}
began=time.monotonic()
result=solve_window(root,place['place'],next(p['frames'] for p in frames['places'] if p['place']==place['place']),
                    required,output,[Path(p['report']) for p in place['coupled_products']])
(output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('place','before','after','connections','weak_components',
             'requested_weak_components','native_accepted','previous_roadbeds_preserved','elapsed_ms')}),flush=True)
print('prototype-total-seconds',time.monotonic()-began,flush=True)
