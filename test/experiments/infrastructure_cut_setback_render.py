"""Show road-cut corrections with unchanged floors and complete adjacent pieces."""

import argparse
import json
from pathlib import Path

import shapely

from infrastructure_cut_setback_experiment import digest,prepared_network
from infrastructure_cut_setbacks import restore_cut_plan
from infrastructure_junction_surfaces import motor_surface
from infrastructure_junction_turn_experiment import render


def stored_view(network,module,report,paths):
    shape = network.shape(module)
    surface = motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                            network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    poses,_ = network.poses(module,{})
    lines = [None if p is None else shapely.LineString(p) for p in paths]
    assert len(report['turns'])==len(lines)
    return shape,surface,poses,lines,report['turns']


def comparison_views(root,result,frames):
    before,after = [],[]
    successful_places = sum(bool(p['plan']['changes']) for p in result['places'])
    for row in result['places']:
        baseline_path = root/row['reference']
        assert digest(baseline_path)==row['reference_sha256']
        baseline = json.loads(baseline_path.read_text())
        original = {r['module']:r for r in baseline['modules']}
        old_paths = {r['module']:r['paths'] for r in baseline['paths']}
        current = {r['module']:r for r in row['changed_modules']}
        new_paths = {r['module']:r['paths'] for r in row['changed_paths']}
        candidates = sorted({c['junction'] for c in row['plan']['changes']},
                            key=lambda m:(-(current[m]['accepted']-original[m]['accepted']),m))
        selected = candidates[:1]
        if row['place']=='Wien' and successful_places<3:
            selected = candidates[:2]
        if not selected:
            continue
        poses = next(p['frames'] for p in frames['places'] if p['place']==row['place'])
        old = prepared_network(root,baseline,poses)
        new = prepared_network(root,baseline,poses)
        restore_cut_plan(new,row['plan'])
        for module in selected:
            before.append((row['place']+' before',module,original[module],stored_view(old,module,original[module],old_paths[module])))
            after.append((row['place']+' after',module,current[module],stored_view(new,module,current[module],new_paths[module])))
    return before+after


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--result',type=Path,default=Path('build/experiments/infrastructure/cut-setbacks/cut-setbacks.json'))
    parser.add_argument('--output',type=Path,default=Path('build/shots/experiments/infrastructure/shared-cut-setbacks.png'))
    args = parser.parse_args()
    result = json.loads(args.result.read_text())
    code = Path(__file__).parent
    for name,value in result['implementation_sha256'].items():
        assert digest(code/name)==value,'the measured implementation changed'
    frame_path = args.input/result['frame_reference']
    assert digest(frame_path)==result['frame_reference_sha256']
    views = comparison_views(args.input,result,json.loads(frame_path.read_text()))
    assert views,'no successful shared-cut correction to render'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    render(args.output,views,'Shared-cut setback: same road surface; all prior and adjacent car routes retained')


if __name__=='__main__':
    main()
