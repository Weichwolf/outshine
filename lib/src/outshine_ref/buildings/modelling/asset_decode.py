"""Use standard asset decoders while retaining authored glTF channels and material precision."""

import base64
import hashlib
import io
import json
from pathlib import Path

from pygltflib import GLTF2
import trimesh
from trimesh.exchange import gltf,obj

from .metadata import Material
from .asset_resources import ImageResource,Resources,gltf_resources


def flattened(values,prefix=''):
    result={}
    for key,value in values.items():
        name=prefix+key
        if value is None:continue
        if isinstance(value,dict):result.update(flattened(value,name+'.'))
        elif isinstance(value,list) and any(isinstance(item,dict) for item in value):
            result[name]=json.dumps(value,sort_keys=True,separators=(',',':'))
        else:result[name]=value
    return result


def decode_gltf(path,resolver):
    document=GLTF2().load(str(path));header=json.loads(document.to_json());materials=[];primitives=[];omitted=[]
    if header.get('extensionsRequired'):raise NotImplementedError('Required glTF extensions need explicit decoder admission')
    for index,value in enumerate(header.get('materials',[])):
        materials.append(Material(value.get('name',str(index)),flattened(value)|{'shader':'CityEnginePBRShader'}))
    for mesh_index,mesh in enumerate(header.get('meshes',[])):
        accepted=[]
        for primitive_index,primitive in enumerate(mesh.get('primitives',[])):
            mode=primitive.get('mode',4)
            if mode==6:raise NotImplementedError('glTF triangle-fan surface decoding remains pending')
            if mode not in range(6):raise ValueError('Invalid glTF primitive mode')
            if mode not in (4,5):
                omitted.append((mesh_index,primitive_index,'non-surface primitive'));continue
            attributes=primitive['attributes']
            for name,index in tuple(attributes.items()):
                if name.startswith('TEXCOORD_') or name=='COLOR_0':
                    accessor=header['accessors'][index]
                    if accessor['componentType']!=5126 and not accessor.get('normalized'):
                        raise ValueError('Integer UV/color accessors require normalized values')
                    attributes['_OS_'+name]=index
            primitives.append(primitive.get('material'));accepted.append(primitive)
        mesh['primitives']=accepted
    blob=document.binary_blob()
    resources=gltf_resources(header,blob,resolver)
    if blob:
        for buffer in header.get('buffers',[]):
            if not buffer.get('uri'):buffer['uri']='data:application/octet-stream;base64,'+base64.b64encode(blob).decode()
    data=gltf.load_gltf(io.BytesIO(json.dumps(header).encode()),resolver=resolver,process=False,merge_primitives=False)
    if len(data['geometry'])!=len(primitives):raise ValueError('glTF decoder omitted a supported primitive')
    material_ids=dict(zip(data['geometry'],primitives,strict=True));serial={}
    for edge in data['graph']:
        name=edge.get('geometry')
        if name and data['geometry'][name].get('metadata',{}).get('from_gltf_primitive'):
            key=(edge['frame_from'],name);index=serial.get(key,0);serial[key]=index+1
            edge['frame_to']=str(key[0])+'::'+str(name)+'::'+str(index)
    return data,material_ids,materials,tuple(omitted),resources


def decode_obj(path,resolver):
    with path.open('rb') as stream:
        data=obj.load_obj(stream,resolver=resolver,group_material=False,split_objects=True,split_groups=True)
    materials=[];ids={};images=[];textures=[]
    for name,geometry in data['geometry'].items():
        visual=geometry.get('visual');source=getattr(visual,'material',None)
        if source is None:ids[name]=None;continue
        values=dict(getattr(source,'kwargs',{}))
        values={key:value for key,value in values.items() if key!='image'}
        image=getattr(source,'image',None)
        if image is not None:
            image_name=image.info.get('file_path')
            if not image_name:raise ValueError('OBJ material image has no source reference')
            values['baseColorTexture.index']=len(images)
            images.append(ImageResource(image_name,'image/'+str(image.format).lower(),resolver.get(image_name)))
            textures.append((len(images)-1,None))
        materials.append(Material(getattr(source,'name',name),flattened(values)));ids[name]=len(materials)-1
    return data,ids,materials,(),Resources(tuple(images),tuple(textures),())


def scene(data):
    result=trimesh.Scene(base_frame=data.get('base_frame','world'))
    for name,values in data['geometry'].items():
        if 'faces' not in values:raise ValueError('Asset decoder returned non-surface geometry')
        if 'vertex_normals' in values:
            import numpy as np
            normal=np.asarray(values['vertex_normals'])
            if normal.shape!=np.asarray(values['vertices']).shape or not np.isfinite(normal).all() or np.any(np.linalg.norm(normal,axis=1)==0):
                raise ValueError('Asset declares inconsistent vertex normals')
        result.geometry[name]=trimesh.Trimesh(**(values|{'process':False}))
    for edge in data.get('graph',()):result.graph.update(**edge)
    return result


class Resolver(trimesh.resolvers.FilePathResolver):
    def __init__(self,path):
        super().__init__(str(path));self.dependencies={};self.missing=[]

    def get(self,name):
        try:value=super().get(name)
        except OSError:
            self.missing.append(str(name));raise
        self.dependencies[str(name)]=hashlib.sha256(value).hexdigest()
        return value


def decode(path):
    path=Path(path).resolve();suffix=path.suffix.lower();resolver=Resolver(path)
    if suffix in ('.gltf','.glb'):data,ids,materials,omitted,resources=decode_gltf(path,resolver)
    elif suffix=='.obj':data,ids,materials,omitted,resources=decode_obj(path,resolver)
    else:raise NotImplementedError('Asset file format is not implemented: '+suffix)
    if resolver.missing:raise ValueError('Asset references missing resources: '+', '.join(resolver.missing))
    return scene(data),data,ids,tuple(materials),omitted,resolver.dependencies,resources
