"""Check stable room-light identities, geometric clearance and state changes without camera input."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from interior_light_plan import lights
from showcase_scene import LABELS,cached


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/interior-light-verification.json'));args=parser.parse_args()
    root=Path(__file__).resolve().parents[3]/'build/experiments/buildings';rows=[]
    for label in LABELS:
        plan=cached(label,root/'opening-inspections',root/'roof-plans')['plan'];on=lights(plan);mixed=lights(plan,'mixed');off=lights(plan,'off')
        assert on==lights(plan) and len(on)==sum(map(len,plan['storey_room_ids'])) and not any(row['enabled'] for row in off)
        assert len({row['room_id'] for row in on})==len(on)
        for i,row in enumerate(on):
            room=plan['rooms'][i%len(plan['rooms'])];x,y,z=row['position'];sx,sy=row['size']
            assert room.covers(shapely.box(x-sx/2,y-sy/2,x+sx/2,y+sy/2))
            shift=row['storey']*(plan['body_top_m']-plan['base_z_m'])/plan['storeys']
            assert plan['floor_z_m']+shift<z<plan['ceiling_z_m']+shift
            for key in ('room_id','storey','position','size','radiance'):assert row[key]==mixed[i][key]==off[i][key]
            assert np.isfinite(row['position']).all() and np.isfinite(row['radiance']).all()
        rows.append(dict(label=label,fixtures=len(on),enabled=sum(row['enabled'] for row in on),mixed_enabled=sum(row['enabled'] for row in mixed),
            omissions=[dict(room_id=row['room_id'],reason=row['omitted_reason']) for row in on if row['omitted_reason']]))
    record=revision()|dict(status='pass',cases=rows,limits='Procedural diagnostic fixture locations and light state only; source room geometry unchanged, native light/game state and occupancy remain open')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=len(rows),fixtures=sum(row['fixtures'] for row in rows))))


if __name__=='__main__':main()
