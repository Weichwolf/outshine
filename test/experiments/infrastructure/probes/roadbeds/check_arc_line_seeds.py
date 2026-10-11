"""Check seed endpoints, reversal and rigid-transform invariance."""

from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_arc_line_paths import arc_line_arc_seeds

rng=np.random.default_rng(1678)
checked=0
for _ in range(100):
    angles=rng.uniform(-np.pi,np.pi,2)
    poses=[dict(position=rng.uniform(-20,20,2),heading=np.array([np.cos(t),np.sin(t)])) for t in angles]
    radius=float(rng.uniform(2,8));a,b=poses
    angle=.37;rotation=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
    offset=np.array([12000.,-7000.])
    move=lambda p:dict(position=rotation@p['position']+offset,heading=rotation@p['heading'])
    reverse=lambda p:dict(position=p['position'],heading=-p['heading'])
    transformed=dict(arc_line_arc_seeds(move(a),move(b),radius))
    reversed_paths=dict(arc_line_arc_seeds(reverse(b),reverse(a),radius))
    for kind,path in arc_line_arc_seeds(a,b,radius):
        xy=shapely.get_coordinates(path)
        np.testing.assert_allclose(xy[[0,-1]],[a['position'],b['position']],atol=1e-10,rtol=0)
        expected=shapely.transform(path,lambda points:points@rotation.T+offset)
        assert expected.hausdorff_distance(transformed[kind])<1e-8
        assert path.hausdorff_distance(reversed_paths[-kind[1],-kind[0]])<1e-8
        assert np.isfinite(xy).all() and path.length>0
        checked+=1
print(dict(random_pose_pairs=100,checked_seeds=checked,endpoints_reversal_and_rigid_transforms=True))
