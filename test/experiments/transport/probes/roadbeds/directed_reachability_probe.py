"""Reclassify frozen proved routes without rerunning geometry construction."""

import argparse
from collections import Counter
import json
from pathlib import Path
import sys
import time

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_cut_setback_experiment import prepared_network,digest
from transport_cut_setbacks import restore_cut_plan
from transport_route_connectivity import network_connectivity
from transport_shared_frames import frame_key


def examine(root,row,frames):
    before=json.loads((root/'window-1000-body'/f"{row['place']}-routes.json").read_text())
    cuts=json.loads((root/'cut-setbacks'/f"{row['place']}-cut-setbacks.json").read_text())
    network=prepared_network(root,before,frames)
    restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']}
    records.update((r['module'],r) for r in cuts['changed_modules'])
    for product in row['coupled_products']:
        path=Path(product['report'])
        assert digest(path)==product['report_sha256']
        report=json.loads(path.read_text())
        restore_cut_plan(network,report['cut_plan'])
        for owner in report['affected']:
            poses,_=network.poses(owner,{})
            indices={frame_key(p)[:3]:i for i,p in enumerate(poses)}
            turns=[]
            for accepted,requests in ((True,report['proofs']),(False,report['unresolved'])):
                for request in requests:
                    if request['module']!=owner:continue
                    a,b=(indices[tuple(key)] for key in request['request'])
                    turns.append(dict(entry=a,exit=b,accepted=accepted))
            records[owner]=dict(module=owner,turns=turns,accepted=sum(t['accepted'] for t in turns),connections=len(turns))
    for turn in row['trials']:
        if turn['accepted']:
            matches=[t for t in records[turn['module']]['turns']
                     if (t['entry'],t['exit'])==(turn['entry'],turn['exit'])]
            assert len(matches)==1
            matches[0]['accepted']=True
    report=dict(place=row['place'],modules=[records[r['module']] for r in before['modules']])
    assert sum(t['accepted'] for r in report['modules'] for t in r['turns'])==row['after']
    began=time.perf_counter()
    result=network_connectivity(network,report)
    result['classification_ms']=(time.perf_counter()-began)*1000
    result['missing_by_module']=dict(Counter(route['module'] for e in result['missing_edges']
        if e['reason']!='already_reachable' for route in e['routes']))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    root=Path('build/experiments/transport')
    source=json.loads(args.report.read_text())
    frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
    results=[]
    for row in source['places']:
        result=examine(root,row,next(p['frames'] for p in frames['places'] if p['place']==row['place']))
        results.append(result)
        print(json.dumps({k:result[k] for k in ('place','missing_edge_reasons','classification_ms','missing_by_module')}),flush=True)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(dict(source=str(args.report),source_sha256=digest(args.report),
        scope='Frozen accepted route graph; no new geometry proof.',places=results),indent=2)+'\n')


if __name__=='__main__':main()
