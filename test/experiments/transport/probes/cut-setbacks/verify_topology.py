import json
import sys
from pathlib import Path

sys.path.insert(0,str(Path('test/experiments/transport').resolve()))
from transport_cut_setback_experiment import digest,prepared_network
from transport_cut_setbacks import restore_cut_plan
from transport_shared_frames import frame_key


def graph_edges(network,rows,mapping=None):
    nodes,requests,proved = set(),set(),set()
    for row in rows:
        poses,_ = network.poses(row['module'],{})
        keys = []
        for p in poses:
            key = frame_key(p)
            if mapping is not None and p['section'] in mapping:
                key = mapping[p['section']][(p['edge'],p['direction'])]
            keys.append(key)
            nodes.add(key)
        for turn in row['turns']:
            edge = keys[turn['entry']],keys[turn['exit']]
            requests.add(edge)
            if turn['accepted']:
                proved.add(edge)
    return nodes,requests,proved


root = Path('build/experiments/transport')
source = root/'cut-setbacks/cut-setbacks.json'
result = json.loads(source.read_text())
frame_path = root/result['frame_reference']
assert digest(frame_path)==result['frame_reference_sha256']
frames = json.loads(frame_path.read_text())
for name,value in result['implementation_sha256'].items():
    assert digest(Path('test/experiments/transport')/name)==value
for row in result['places']:
    baseline_path = root/row['reference']
    assert digest(baseline_path)==row['reference_sha256']
    baseline = json.loads(baseline_path.read_text())
    network = prepared_network(root,baseline,next(p['frames'] for p in frames['places'] if p['place']==row['place']))
    before = graph_edges(network,baseline['modules'])
    mapping = {c['section']:{} for c in row['plan']['changes']}
    for module in baseline['modules']:
        for pose in network.poses(module['module'],{})[0]:
            if pose['section'] in mapping:
                group = mapping[pose['section']]
                contact = pose['edge'],pose['direction']
                if contact in group:
                    assert group[contact]==frame_key(pose)
                group[contact] = frame_key(pose)
    restore_cut_plan(network,row['plan'])
    changed = {r['module']:r for r in row['changed_modules']}
    current = [changed.get(r['module'],r) for r in baseline['modules']]
    after = graph_edges(network,current,mapping)
    assert before[0]==after[0], 'source contact identity changed'
    assert before[1]==after[1], 'requested directed source graph changed'
    assert before[2]<=after[2], 'a proved route disappeared'
    assert row['before_graph']['requested_weak_components']==row['after_graph']['requested_weak_components']
    assert row['before_graph']['requested_strong_components']==row['after_graph']['requested_strong_components']
    print(json.dumps(dict(place=row['place'],report_sha256=digest(source),nodes=len(before[0]),
        requested_edges=len(before[1]),previous_proved_edges=len(before[2]),new_proved_edges=len(after[2]-before[2]),
        identical_requested_graph=True,all_previous_proved_edges_preserved=True)),flush=True)
