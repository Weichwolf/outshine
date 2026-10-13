"""Independent controls for normalized appearance and transformed normal orientation."""

from pathlib import Path

import numpy as np
from pygltflib import Accessor,BufferView,GLTF2

from modelling_insert import Asset
from outshine_ref.buildings.modelling.scope import Scope
from modelling_shape import Shape
from verify_modelling_extrude import near


def body(asset):
    return Shape(asset.geometry,Scope(position=asset.minimum,size=asset.geometry.reference_size),asset=asset)


def append_channel(document,values,kind,component):
    raw=document.binary_blob();offset=(len(raw)+3)//4*4;encoded=values.tobytes()
    document.set_binary_blob(raw+b'\0'*(offset-len(raw))+encoded)
    document.buffers[0].byteLength=offset+len(encoded)
    document.bufferViews.append(BufferView(buffer=0,byteOffset=offset,byteLength=len(encoded)))
    document.accessors.append(Accessor(bufferView=len(document.bufferViews)-1,componentType=component,
                                      count=len(values),type=kind,normalized=True))
    return len(document.accessors)-1


def controls(output,glb_path):
    document=GLTF2().load(str(glb_path));primitive=document.meshes[0].primitives[0]
    colours=np.array([[0,128,255,64],[255,16,32,255],[64,32,16,128],[127,129,1,0]],dtype=np.uint8)
    coordinates=np.array([[0,65535],[32768,16],[65535,32768],[64,128]],dtype='<u2')
    primitive.attributes.COLOR_0=append_channel(document,colours,'VEC4',5121)
    primitive.attributes.TEXCOORD_1=append_channel(document,coordinates,'VEC2',5123)
    path=Path(output)/'normalized-channels.glb';document.save_binary(str(path));asset=Asset.from_file(path)
    mesh=body(asset).emit();near(mesh['vertex_colors'],np.tile(colours/255.,(2,1)))
    near(mesh['uv1'],np.tile(coordinates/65535.,(2,1)))
    for child in body(asset).comp('g'):
        near(child.emit()['vertex_colors'],colours/255.);near(child.emit()['uv1'],coordinates/65535.)
    operations=[lambda:body(asset).extrude(1),lambda:body(asset).split('x',1),
                lambda:body(asset).split_area('x',1),lambda:body(asset).comp('e')]
    for operation in operations:
        try:operation()
        except NotImplementedError:pass
        else:raise AssertionError('Additional appearance channel was silently dropped')
    document=GLTF2().load(str(glb_path))
    node=next(node for node in document.nodes if node.name=='pane-a')
    matrix=np.array([[2.,0.,1.,1.],[.5,3.,0.,2.],[1.,0.,1.,3.],[0.,0.,0.,1.]])
    node.matrix=matrix.T.ravel().tolist();node.translation=[];node.rotation=[];node.scale=[]
    path=Path(output)/'nonuniform-normals.glb';document.save_binary(str(path));mesh=body(Asset.from_file(path)).emit()
    corners=mesh['vertices'][mesh['faces']]
    expected=np.cross(corners[:,1]-corners[:,0],corners[:,2]-corners[:,0])
    expected/=np.linalg.norm(expected,axis=1)[:,None]
    near(mesh['normals'][mesh['faces']],np.repeat(expected[:,None,:],3,axis=1))
    return dict(normalized_uint8_colors=True,normalized_uint16_uv=True,component_channels=True,
                unsupported_edits_rejected=len(operations),inverse_transpose_normals=True)
