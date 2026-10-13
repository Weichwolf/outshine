"""Owned import controls for source channels, scene hierarchy, materials and reproducibility."""

import argparse
from copy import deepcopy
from dataclasses import asdict
import hashlib
import json
from pathlib import Path

import numpy as np
from pygltflib import GLTF2

from evidence import revision
from modelling_asset_cases import COLOUR,UV,VERTICES,fixtures
from modelling_insert import Asset
from outshine_ref.buildings.modelling.scope import Scope
from modelling_shape import Shape
from verify_modelling_extrude import near
from verify_modelling_asset_channels import controls


def emitted(asset):
    return Shape(asset.geometry,Scope(position=asset.minimum,size=asset.geometry.reference_size),asset=asset).emit()


def obj_control(path):
    asset=Asset.from_file(path);mesh=emitted(asset)
    near(mesh['vertices'],VERTICES);near(mesh['uv'],UV);near(mesh['normals'],[[0,0,1]]*4)
    assert len(mesh['faces'])==2 and asset.geometry.group_names==((0,'pane_facade'),)
    values=dict(asset.geometry.material_defs[1][1].attributes);near(values['kd'],COLOUR[:3])
    assert 'owned-pane.mtl' in dict(asset.import_info.dependencies)
    return dict(name=path.name,groups=asset.geometry.group_names,source=asdict(asset.import_info))


def gltf_control(path,matrices):
    asset=Asset.from_file(path);mesh=emitted(asset);materials=dict(asset.geometry.material_defs)
    near(dict(materials[1].attributes)['pbrMetallicRoughness.baseColorFactor'],COLOUR)
    for group,matrix in enumerate(matrices):
        face_rows=np.flatnonzero(mesh['groups']==group);used=np.unique(mesh['faces'][face_rows])
        expected=VERTICES@matrix[:3,:3].T+matrix[:3,3]
        near(mesh['vertices'][used],expected);near(mesh['uv'][used],UV*[1,-1]+[0,1],tolerance=1e-6)
        near(mesh['uv1'][used],mesh['uv'][used])
        triangles=mesh['vertices'][mesh['faces'][face_rows]]
        normals=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
        assert np.all(normals[:,2]>0), 'Reflection did not preserve front orientation'
        near(mesh['normals'][used],[[0,0,1]]*4)
    assert all(node[3]=='provided' for node in asset.import_info.nodes)
    assert len(asset.resources.images)==1 and len(asset.resources.images[0].encoded)>0
    again=Asset.from_file(path)
    assert asset.geometry.polygons==again.geometry.polygons and asset.import_info==again.import_info
    for key,value in mesh.items():assert np.array_equal(value,emitted(again)[key])
    assert asset.geometry.material_defs==again.geometry.material_defs
    body=Shape(asset.geometry,Scope(position=asset.minimum,size=asset.geometry.reference_size),asset=asset)
    for component in body.comp('g'):
        assert 'uv1' in component.emit() and len(component.emit()['uv1'])==len(component.emit()['vertices'])
    return dict(name=path.name,source=asdict(asset.import_info),images=[dict(name=image.name,bytes=len(image.encoded),
        sha256=hashlib.sha256(image.encoded).hexdigest()) for image in asset.resources.images])


def invalid_controls(output,obj_path,glb_path):
    missing=output/'missing-resource.obj';missing.write_text(obj_path.read_text().replace('owned-pane.mtl','absent.mtl'))
    document=GLTF2().load(str(glb_path));document.extensionsRequired=['OUTSHINE_unsupported_control']
    extension=output/'required-extension.glb';document.save_binary(str(extension))
    zero=GLTF2().load(str(glb_path));zero.nodes[0].scale=[0.,0.,0.];zero.nodes[0].matrix=[]
    singular=output/'singular.glb';zero.save_binary(str(singular))
    fan=GLTF2().load(str(glb_path));fan.meshes[0].primitives[0].mode=6
    fan_path=output/'triangle-fan.glb';fan.save_binary(str(fan_path))
    calls=[lambda:Asset.from_file(output/'unsupported.fbx'),lambda:Asset.from_file(missing),
           lambda:Asset.from_file(extension),lambda:Asset.from_file(singular),lambda:Asset.from_file(fan_path)]
    for call in calls:
        try:call()
        except (ValueError,NotImplementedError,FileNotFoundError):pass
        else:raise AssertionError('Invalid asset import accepted')
    return len(calls)


def additional_controls(output,glb_path):
    document=GLTF2().load(str(glb_path));primitive=document.meshes[0].primitives[0]
    document.meshes[0].primitives.append(deepcopy(primitive))
    path=output/'multiple-primitives.glb';document.save_binary(str(path))
    a,b=Asset.from_file(path),Asset.from_file(path)
    assert len(a.import_info.nodes)==4 and a.import_info==b.import_info
    assert a.geometry.group_names==b.geometry.group_names and len(a.geometry.fields['faces'])==8
    document=GLTF2().load(str(glb_path));document.materials=[]
    primitive=document.meshes[0].primitives[0];primitive.material=None;primitive.attributes.NORMAL=None
    path=output/'uv-without-material.glb';document.save_binary(str(path));asset=Asset.from_file(path)
    assert 'uv' in asset.geometry.fields and 'uv1' in asset.geometry.fields
    assert all(node[3]=='library-derived' for node in asset.import_info.nodes)
    assert all(face.material==0 for face in asset.geometry.polygons)
    return dict(multiple_primitives_deterministic=True,uv_without_material=True,missing_normals_derived=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--files',type=Path,default=Path('build/experiments/buildings/asset-import-controls'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/asset-import-verification.json'))
    args=parser.parse_args();obj_path,paths,matrices=fixtures(args.files)
    rows=[obj_control(obj_path),*[gltf_control(path,matrices) for path in paths]]
    record=revision()|dict(status='pass',cases=rows,additional=additional_controls(args.files,paths[0]),channels=controls(args.files,paths[0]),invalid=invalid_controls(args.files,obj_path,paths[0]),
        limits='Owned OBJ/glTF/GLB controls only; source file precision, hierarchy/reflections, UV0/UV1, normals, image storage and repeated import verified. Other formats, required extensions, skins/animations, shader rendering and extra-channel editing remain open.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',cases=len(rows),invalid=record['invalid'])))


if __name__=='__main__':main()
