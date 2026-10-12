from pathlib import Path
import json
import numpy as np,shapely
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection,LineCollection
from transport_flat_surfaces import load_graphs
from transport_port_lanes import junction_interfaces,module_face_index,module_shape,lane_poses
from transport_vehicle_sweep import body_outlines

root=Path('build/experiments/transport');name='Wien';edge=8
anchor=np.array([70.75397562,15.96424879])
vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
graph=next(g for g in load_graphs(root/'directed-graphs'/f'{name}-flat-network.npz',json.loads((root/'directed-graphs'/f'{name}-flat-network.json').read_text())) if g['tier']==['land',0,'ground'])
fig,axes=plt.subplots(1,2,figsize=(12,6),constrained_layout=True)
for ax,folder,title in zip(axes,('baselines/before-vehicle-cuts','directed-surfaces'),('Before: footway cut at the motor dead end','After: common cut moved to usable vehicle poses')):
    with np.load(root/folder/f'{name}-flat-surfaces.npz') as a:product={k:a[k] for k in a.files}
    faces=module_face_index(product);poses=[]
    for module,ports in junction_interfaces(product).items():
        local=[p for p in ports if shapely.intersects(p['line'],graph['lines'][edge])]
        if not local:continue
        found,_=lane_poses(graph,local,module_shape(product,module,faces),vehicle)
        poses.extend(p for p in found if p['edge']==edge and p['direction']==1)
    pose=min(poses,key=lambda p:np.linalg.norm(p['position']-anchor))
    xy=product['vertices'][product['indices']]
    keep=np.all(xy.max(axis=1)>=anchor-[18,12],axis=1)&np.all(xy.min(axis=1)<=anchor+[8,9],axis=1)
    core=product['part_roles'][product['face_owners'][keep]].astype(bool)
    ax.add_collection(PolyCollection(xy[keep],facecolors=np.where(core[:,None],np.array([.43,.65,.55,1]),np.array([.78,.80,.77,1])),edgecolors='none'))
    seams=product['vertices'][product['port_edges']]
    keep=np.all(seams.max(axis=1)>=anchor-[18,12],axis=1)&np.all(seams.min(axis=1)<=anchor+[8,9],axis=1)
    ax.add_collection(LineCollection(seams[keep],colors='#a87428',linewidths=1.8))
    angle=np.arctan2(pose['heading'][1],pose['heading'][0])
    body=body_outlines(pose['position'][None,:],np.array([angle]),.9,3.3,1.1)[0]
    ax.fill(*body.T,facecolor='none',edgecolor='#bd5744' if folder.startswith('baselines/') else '#167dba',linewidth=2.)
    ax.plot(*shapely.get_coordinates(graph['lines'][edge]).T,linestyle=':',color='#53645e',linewidth=1.)
    ax.set(xlim=(anchor[0]-18,anchor[0]+8),ylim=(anchor[1]-12,anchor[1]+9),aspect='equal',facecolor='#f1f0e8',xlabel='East [m]',ylabel='North [m]',title=title)
fig.suptitle('Wien: same physical roads; shared cuts respect the public motor vehicle envelope')
fig.savefig('build/shots/experiments/transport/vehicle-cut-clearance.png',dpi=140);plt.close(fig)
