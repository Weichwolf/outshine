from pathlib import Path
import json
import numpy as np
import shapely
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection,LineCollection
from transport_flat_surfaces import load_graphs
from transport_port_lanes import junction_interfaces,module_face_index,module_shape,lane_poses
from transport_vehicle_sweep import body_outlines

root=Path('build/experiments/transport')
fig,axes=plt.subplots(2,3,figsize=(15,9),constrained_layout=True)
for column,(name,module,port) in enumerate((('Wien',1745,0),('ZuerichHauptbahnhof',1272,4),('BaselBadischerBahnhof',838,0))):
    old=root/'baselines/before-remainder-cuts'/f'{name}-flat-surfaces.npz'
    with np.load(old) as a:base={k:a[k] for k in a.files}
    graph=next(g for g in load_graphs(root/'directed-graphs'/f'{name}-flat-network.npz',json.loads((root/'directed-graphs'/f'{name}-flat-network.json').read_text())) if g['tier']==['land',0,'ground'])
    shape=module_shape(base,module,module_face_index(base))
    ports=junction_interfaces(base)
    vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
    poses,_=lane_poses(graph,ports[module],shape,vehicle)
    pose=poses[port];point=pose['position'];angle=np.arctan2(pose['heading'][1],pose['heading'][0])
    body=body_outlines(point[None,:],np.array([angle]),.9,3.3,1.1)[0]
    with np.load(root/'port-span-trial/directed-surfaces'/old.name) as a:after={k:a[k] for k in a.files}
    for row,data in enumerate((base,after)):
        ax=axes[row,column];xy=data['vertices'][data['indices']]
        keep=np.all(xy.max(axis=1)>=point-7,axis=1)&np.all(xy.min(axis=1)<=point+7,axis=1)
        core=data['part_roles'][data['face_owners'][keep]].astype(bool)
        ax.add_collection(PolyCollection(xy[keep],facecolors=np.where(core[:,None],np.array([.43,.65,.55,1]),np.array([.78,.80,.77,1])),edgecolors='none'))
        seams=data['vertices'][data['port_edges']]
        keep=np.all(seams.max(axis=1)>=point-7,axis=1)&np.all(seams.min(axis=1)<=point+7,axis=1)
        ax.add_collection(LineCollection(seams[keep],colors='#a87428',linewidths=2.))
        if row==0:
            ax.fill(*body.T,facecolor='none',edgecolor='#bd5744',linewidth=2.)
            ax.plot(*point,'o',color='#bd5744',markersize=4)
        ax.set(xlim=(point[0]-7,point[0]+7),ylim=(point[1]-7,point[1]+7),aspect='equal',facecolor='#f1f0e8',xlabel='East [m]',ylabel='North [m]',title=f'{name}: '+('old cut; invalid endpoint body' if row==0 else 'terminal stub inside junction'))
fig.suptitle('Same physical roads; green junctions / grey corridors / orange shared cuts; short terminal seams removed')
path=Path('build/shots/experiments/transport/terminal-port-cuts.png');fig.savefig(path,dpi=140);plt.close(fig)
