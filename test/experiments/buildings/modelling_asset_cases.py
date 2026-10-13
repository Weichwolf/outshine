"""Owned OBJ/glTF controls with seams, two scene instances and precise material values."""

from pathlib import Path

import numpy as np
from pygltflib import GLTF2
import trimesh


UV=np.array([[.1,.2],[.9,.2],[.9,.8],[.1,.8]])
VERTICES=np.array([[0.,0.,0.],[1.,0.,0.],[1.,1.,0.],[0.,1.,0.]])
COLOUR=(.147123456,.512345678,.812345678,.73)


def obj_control(output):
    path=output/'owned-pane.obj'
    path.write_text('mtllib owned-pane.mtl\no pane\ng facade\n'
        'v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n'
        'vt 0.1 0.2\nvt 0.9 0.2\nvt 0.9 0.8\nvt 0.1 0.8\nvn 0 0 1\nusemtl facade\n'
        'f -4/-4/-1 -3/-3/-1 -2/-2/-1\nf -4/-4/-1 -2/-2/-1 -1/-1/-1\n')
    (output/'owned-pane.mtl').write_text('newmtl facade\nKd 0.147123456 0.512345678 0.812345678\nd 0.73\n')
    return path


def gltf_controls(output):
    mesh=trimesh.Trimesh(VERTICES,[[0,1,2],[0,2,3]],vertex_normals=np.tile([0.,0.,1.],(4,1)),process=False,
                        visual=trimesh.visual.texture.TextureVisuals(uv=UV))
    scene=trimesh.Scene();rig=trimesh.transformations.rotation_matrix(np.pi/4,[0,0,1]);rig[:3,3]=[3,4,5]
    scene.graph.update(frame_to='rig',matrix=rig)
    a=np.diag([2.,3.,1.,1.]);a[:3,3]=[1,2,3]
    b=np.diag([-1.5,2.,1.,1.]);b[:3,3]=[-2,1,4]
    scene.add_geometry(mesh,node_name='pane-a',geom_name='pane',parent_node_name='rig',transform=a)
    scene.graph.update(frame_from='rig',frame_to='pane-b',geometry='pane',matrix=b)
    document=GLTF2.load_from_bytes(scene.export(file_type='glb',include_normals=True));primitive=document.meshes[0].primitives[0]
    primitive.attributes.TEXCOORD_1=primitive.attributes.TEXCOORD_0
    document.materials[primitive.material].pbrMetallicRoughness.baseColorFactor=list(COLOUR)
    paths=[]
    path=output/'owned-hierarchy.glb';document.save_binary(str(path));paths.append(path)
    path=output/'owned-hierarchy.gltf';document.save_json(str(path));paths.append(path)
    from pygltflib import BufferFormat
    document.convert_buffers(BufferFormat.DATAURI);path=output/'owned-hierarchy-embedded.gltf';document.save_json(str(path));paths.append(path)
    return paths,(rig@a,rig@b)


def fixtures(output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True);obj=obj_control(output);gltf,transforms=gltf_controls(output)
    return obj,gltf,transforms
