"""Workbench texture inspection of emitted UVs; no PBR or transparency admission."""

import hashlib
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
import numpy as np


def render(config):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    source=Path(config['mesh']);texture=Path(config['texture'])
    if hashlib.sha256(source.read_bytes()).hexdigest()!=config['mesh_sha256'] or hashlib.sha256(texture.read_bytes()).hexdigest()!=config['texture_sha256']:
        raise ValueError('UV inspection input changed')
    data=dict(np.load(source));mesh=bpy.data.meshes.new('emitted-uv-inspection')
    mesh.from_pydata(data['vertices'].tolist(),[],data['faces'].tolist());mesh.update()
    layer=mesh.uv_layers.new(name='selected-source-uv')
    coordinates=data[config['uv_channel']][data['faces']].reshape(-1,2)
    layer.data.foreach_set('uv',coordinates.ravel());readback=np.empty(coordinates.size,dtype=np.float32);layer.data.foreach_get('uv',readback)
    if not np.allclose(readback.reshape(-1,2),coordinates,atol=1e-6,rtol=1e-6):raise ValueError('Blender UV binding differs from emitted coordinates')
    material=bpy.data.materials.new('owned-asymmetric-control-texture');material.use_nodes=True
    image=material.node_tree.nodes.new('ShaderNodeTexImage');image.image=bpy.data.images.load(str(texture));image.interpolation='Closest';image.extension='REPEAT'
    material.node_tree.nodes.active=image;material.node_tree.links.new(image.outputs['Color'],material.node_tree.nodes.get('Principled BSDF').inputs['Base Color'])
    mesh.materials.append(material);obj=bpy.data.objects.new('emitted-surfaces',mesh);bpy.context.collection.objects.link(obj)
    for label in config.get('labels',()):
        curve=bpy.data.curves.new('control-label','FONT');curve.body=label['text'];curve.size=.23
        item=bpy.data.objects.new('control-label',curve);bpy.context.collection.objects.link(item)
        item.location=label['position'];item.rotation_euler=(np.pi/2.,0.,0.)
        ink=bpy.data.materials.new('label-ink');ink.diffuse_color=(.035,.04,.045,1.);curve.materials.append(ink)
    scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH'
    shading=scene.display.shading;shading.light='FLAT';shading.color_type='TEXTURE';shading.show_shadows=False
    shading.show_cavity=False;shading.show_specular_highlight=False;shading.background_type='WORLD'
    scene.world.color=(.8,.8,.8);scene.view_settings.view_transform='Standard'
    bpy.ops.object.camera_add(location=config['eye']);camera=bpy.context.object
    camera.rotation_euler=(Vector(config['target'])-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=config['ortho_scale'];camera.data.clip_end=1000.;scene.camera=camera
    width,height=config['profile'];scene.render.resolution_x=width;scene.render.resolution_y=height;scene.render.resolution_percentage=100
    bpy.context.view_layer.update()
    points=data['vertices'][np.unique(data['faces'])]
    projected=np.asarray([world_to_camera_view(scene,camera,Vector(point))[:] for point in points])
    if np.any(projected[:,:2]<.01) or np.any(projected[:,:2]>.99) or np.any(projected[:,2]<=0):
        raise ValueError('UV inspection camera crops the source geometry')
    scene.render.image_settings.file_format='PNG';scene.render.filepath=config['output'];bpy.ops.render.render(write_still=True)
    record=dict(renderer=bpy.app.version_string,engine=scene.render.engine,profile=[width,height],texture_shader='Workbench flat image texture with repeat image node; no PBR, lighting or alpha admission',
                triangles=len(data['faces']),uv_binding_verified=True,projected_bounds=projected[:,:2].min(0).tolist()+projected[:,:2].max(0).tolist(),
                mesh_sha256=config['mesh_sha256'],texture_sha256=config['texture_sha256'])
    Path(config['output']).with_suffix('.renderer.json').write_text(json.dumps(record,indent=2)+'\n')


if __name__=='__main__':render(json.loads(Path(sys.argv[sys.argv.index('--')+1]).read_text()))
