import argparse,json,sys
from pathlib import Path
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_cut_setback_experiment import prepared_network
from infrastructure_cut_setbacks import restore_cut_plan
from infrastructure_coupled_roadbed_products import apply_coupled_product
parser=argparse.ArgumentParser()
parser.add_argument('--product',type=Path,required=True)
parser.add_argument('--reference',type=Path,required=True)
args=parser.parse_args()
report=json.loads(args.product.read_text())
root=Path('build/experiments/infrastructure');name=report['place']
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
reference=next(p for p in json.loads(args.reference.read_text())['places'] if p['place']==name)
required={(t['module'],t['entry'],t['exit']) for t in reference['trials'] if t['accepted']}
trial,after,remapped,note=apply_coupled_product(network,records,required,root,args.product,set())
assert len(required)==len(remapped)
print(json.dumps(dict(modules={owner:dict(accepted=after[owner]['accepted'],connections=after[owner]['connections'])
                              for owner in report['affected'] if owner in after},
                      outside_window=sorted(set(report['affected'])-after.keys()),
                      proved_routes=note['proved_routes'],required=len(required),retained=len(remapped))))
sections={row['section'] for row in report['cut_plan']['sections']}
if set(report.get('changed_frame_sections',sections))-sections:
    report['changed_frame_sections']=sorted(sections)
    negative=args.product.with_name('missing-frame-scope.json')
    negative.write_text(json.dumps(report))
    try:
        try:
            apply_coupled_product(network,records,required,root,negative,set())
        except AssertionError as error:
            assert str(error)=='external frame changed',str(error)
            print('undeclared frame change rejected')
        else:
            raise AssertionError('undeclared frame change accepted')
    finally:
        negative.unlink()
