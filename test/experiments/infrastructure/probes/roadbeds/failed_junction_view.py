"""Inspect the first constructive attempt without weakening any acceptance check."""

import argparse
import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_building_clearance import plot_geometry

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--place',required=True)
parser.add_argument('--module',type=int,required=True)
parser.add_argument('--pair',type=int,nargs=2,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
original=coupled.construct_module
products={}


def capture(*values,**kwargs):
    result=original(*values,**kwargs)
    products.setdefault(values[2],result)
    return result


coupled.construct_module=capture
try:
    coupled.real_case(Path('build/experiments/infrastructure'),args.place,args.module,args.pair,4.,args.output)
except AssertionError as error:
    print('rejected:',str(error))
product=products[args.module]
print(json.dumps([dict(pair=r['pair'],reason=r['reason'],details=r['details']) for r in product['unresolved']]))
(args.output/'unresolved.json').write_text(json.dumps(product['unresolved'],indent=2)+'\n')
import matplotlib.pyplot as plt
fig,ax=plt.subplots(figsize=(9,8),constrained_layout=True)
ax.set_facecolor('#f3eee1')
plot_geometry(ax,[product['base']],'#8d9993')
for row in product['unresolved']:
    xy=np.array(row['source_path'])
    ax.plot(xy[:,0],xy[:,1],'--',label=f"{row['entry']} → {row['exit']}")
    for body in row['endpoint_bodies']:
        xy=np.array(body);ax.fill(xy[:,0],xy[:,1],color='#b54a49',alpha=.2)
for index,pose in enumerate(product['poses']):
    xy=pose['position'];heading=pose['heading']
    ax.annotate(str(index),xy=xy,xytext=(4,4),textcoords='offset points')
    ax.arrow(*xy,*(2*heading),width=.06,color='#234c6a')
ax.set_aspect('equal');ax.legend();ax.set_xlabel('east [m]');ax.set_ylabel('north [m]')
ax.set_title(f'{args.place} · {args.module} · rejected first attempt; dashed source routes')
fig.savefig(args.output/'diagnostic.png',dpi=150)
