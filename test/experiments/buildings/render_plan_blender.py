"""Blender Workbench inspections of selected storeys or constructed core components."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy
import gpu
from mathutils import Vector
import numpy as np


COLOURS = [(0.72, .64, .50, 1.), (.40, .43, .46, 1.), (.65, .69, .74, 1.), (.21, .61, .75, 1.), (.76, .80, .82, 1.),
           (.68,.52,.34,1.), (.77,.67,.52,1.), (.31,.46,.42,1.)]


def render(row):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    data = np.load(row['mesh'])
    selected = data['materials'] != 2
    view = row.get('view','storey')
    if 'levels' in data:
        if view == 'core':
            selected &= np.isin(data['materials'],[5,6,7]) & (data['levels'] <= 1)
        else:
            selected &= data['levels'] == row.get('level',0)
    faces, colours = data['faces'][selected],data['materials'][selected]
    geometry = bpy.data.meshes.new('selected-source-mesh')
    geometry.from_pydata(data['vertices'].tolist(), [], faces.tolist())
    geometry.update()
    building = bpy.data.objects.new('inspection-components', geometry)
    scene.collection.objects.link(building)
    for index, colour in enumerate(COLOURS):
        material = bpy.data.materials.new('component-'+str(index))
        material.diffuse_color = colour
        geometry.materials.append(material)
    for polygon, material in zip(geometry.polygons, colours):
        polygon.material_index = int(material)
    points = data['vertices'][np.unique(faces)]
    lower, upper = points.min(axis=0), points.max(axis=0)
    target = Vector(((lower+upper)/2).tolist())
    tangent = row.get('camera_tangent', [1., 0.])
    normal = [-tangent[1], tangent[0]]
    direction = Vector((tangent[0]*.26-normal[0]*.42, tangent[1]*.26-normal[1]*.42,
                        .40 if view == 'core' else .87)).normalized()
    camera_data = bpy.data.cameras.new('inspection-camera')
    camera = bpy.data.objects.new('inspection-camera', camera_data)
    scene.collection.objects.link(camera)
    camera.location = target+direction*max(float(np.linalg.norm(upper-lower))*2, 20.)
    camera.rotation_euler = (target-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type = 'ORTHO'
    camera_data.clip_end = 10000.
    rotation = camera.rotation_euler.to_matrix().transposed()
    projected = np.array([rotation@(Vector(point.tolist())-target) for point in points])
    half_extent = np.abs(projected[:, :2]).max(axis=0)
    camera_data.ortho_scale = max(float(half_extent[0])*2, float(half_extent[1])*2*(800/550))*1.12
    normalized = projected[:, :2]/[camera_data.ortho_scale, camera_data.ortho_scale*(550/800)]+.5
    if normalized.min() < .04 or normalized.max() > .96:
        raise ValueError('Inspection camera crops the source mesh')
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x, scene.render.resolution_y = 800, 550
    scene.render.resolution_percentage = 100
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 2
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = row['output']
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.background_type = 'VIEWPORT'
    scene.display.shading.background_color = (1., 1., 1.)
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'BOTH'
    scene.display.shading.show_specular_highlight = False
    scene.view_settings.view_transform = 'Standard'
    bpy.ops.render.render(write_still=True)
    output = Path(row['output'])
    result = dict(engine=scene.render.engine, blender_version=bpy.app.version_string, profile=[800, 550],
                  ceiling_visibility='hidden for inspection; original mesh unchanged',
                  inspection=dict(view=view, max_core_level=1 if view == 'core' else None,
                                  storey=row.get('level',0) if view == 'storey' else None,
                                  selected_triangles=len(faces), core_view_source_floors_hidden=view == 'core'),
                  camera_bounds_normalized=[normalized.min(axis=0).tolist(), normalized.max(axis=0).tolist()],
                  gpu_vendor=gpu.platform.vendor_get(), gpu_renderer=gpu.platform.renderer_get(),
                  mesh_sha256=hashlib.sha256(Path(row['mesh']).read_bytes()).hexdigest(),
                  image_sha256=hashlib.sha256(output.read_bytes()).hexdigest(), native_gpu_budget_proved=False)
    output.with_suffix('.json').write_text(json.dumps(result, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batch', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    for row in json.loads(args.batch.read_text()):
        render(row)


if __name__ == '__main__':
    main()
