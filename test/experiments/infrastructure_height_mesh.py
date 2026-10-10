"""Clip road footprints onto the solved triangular height lattice."""

import numpy as np
import shapely

from geos_triangulation import triangles_of


def lattice_triangles(xy):
    a,b,c,d = xy[:-1,:-1],xy[:-1,1:],xy[1:,:-1],xy[1:,1:]
    return shapely.polygons(np.stack([np.stack([a,b,c],axis=-2),
                                     np.stack([d,c,b],axis=-2)],axis=-3).reshape(-1,3,2))


def evaluate(xy, height, points, spacing):
    p = (points-xy[0,0])/spacing
    column = np.clip(np.floor(p[:,0]).astype(int),0,height.shape[1]-2)
    row = np.clip(np.floor(p[:,1]).astype(int),0,height.shape[0]-2)
    u,v = p[:,0]-column,p[:,1]-row
    a,b = height[row,column],height[row,column+1]
    c,d = height[row+1,column],height[row+1,column+1]
    z = np.where(u+v <= 1, a+u*(b-a)+v*(c-a),
                 d+(1-u)*(c-d)+(1-v)*(b-d))
    return z,(row,column,(u+v>1).astype(int))


def mesh(xy, heights, tiers, plan, maximum, spacing):
    tiles = lattice_triangles(xy)
    products,reports = {},[]
    for index,tier in enumerate(tiers):
        clipped = shapely.intersection(tiles,plan[tier])
        parts = shapely.get_parts(clipped)
        parts = parts[(shapely.get_type_id(parts)==3) & (shapely.area(parts)>1e-12)]
        triangles = triangles_of(shapely.MultiPolygon(parts.tolist()))
        coordinates = shapely.get_coordinates(triangles).reshape(-1,4,2)[:,:3]
        vertices,indices = np.unique(coordinates.reshape(-1,2),axis=0,return_inverse=True)
        z,_ = evaluate(xy,heights[index],vertices,spacing)
        xyz = np.c_[vertices,z]
        indices = indices.reshape(-1,3)
        faces = xyz[indices]
        normal = np.cross(faces[:,1]-faces[:,0],faces[:,2]-faces[:,0])
        nonzero = abs(normal[:,2])>1e-12
        slopes = np.linalg.norm(normal[nonzero,:2],axis=-1)/abs(normal[nonzero,2])
        _,owners = evaluate(xy,heights[index],faces[nonzero,:,:2].mean(axis=1),spacing)
        excess = np.maximum(slopes-maximum[index][owners],0).max(initial=0)
        area_error = abs(shapely.area(triangles).sum()-plan[tier].area)
        products[tier] = dict(vertices=xyz,indices=indices)
        reports.append(dict(tier=list(tier),vertices=len(vertices),triangles=len(indices),
                            maximum_face_grade=float(slopes.max(initial=0)),
                            maximum_face_grade_error=float(excess),
                            area_error_m2=float(area_error)))
    return products,reports


def render(path, place, products, complete):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    from infrastructure_network import colour
    fig = plt.figure(figsize=(12,8),constrained_layout=True)
    ax = fig.add_subplot(projection='3d')
    for tier,product in products.items():
        faces = product['vertices'][product['indices']]
        shape = Poly3DCollection(faces,facecolor=colour(tier),edgecolor='#27312e',
                                 linewidth=.12,alpha=.5 if tier[1]=='tunnel' else 1)
        ax.add_collection3d(shape)
    vertices = np.concatenate([p['vertices'] for p in products.values()])
    ax.auto_scale_xyz(*vertices.T)
    span = np.ptp(vertices,axis=0)
    ax.set_box_aspect([span[0],span[1],max(span[2],20)*3])
    ax.view_init(elev=38,azim=-110)
    ax.set(xlabel='East [m]',ylabel='North [m]',zlabel='Height [m]',
           title=f'{place}: physical surfaces, height exaggerated 3x; '+
                 ('constraints satisfied' if complete else 'UNRESOLVED constraints'))
    path.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(path,dpi=140)
    plt.close(fig)
