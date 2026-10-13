"""Eevee diagnostic renderer for already emitted Python products; run by preview.py."""

import json
import math
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))

import bpy
from mathutils import Matrix, Vector

from demand import View


def material(name, colour, roughness, metallic=0., object_colour=False):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    result.diffuse_color = (*colour,1.)
    tree = result.node_tree
    shader = tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*colour, 1.)
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Metallic'].default_value = metallic
    if object_colour:
        info = tree.nodes.new('ShaderNodeObjectInfo')
        tree.links.new(info.outputs['Color'], shader.inputs['Base Color'])
    return result


def mix_shaders(tree, first, second, weight):
    node = tree.nodes.new('ShaderNodeMixShader')
    tree.links.new(weight, node.inputs[0])
    tree.links.new(first, node.inputs[1])
    tree.links.new(second, node.inputs[2])
    return node.outputs[0]


def shell_material(name, pixels, colours):
    result = material(name, (1., 1., 1.), .68, object_colour=True)
    tree = result.node_tree
    image = bpy.data.images.new(name+'-weights', width=pixels['size'], height=pixels['size'], float_buffer=True)
    image.colorspace_settings.name = 'Non-Color'
    image.pixels.foreach_set(pixels['rgba'])
    image.update()
    texture = tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    texture.interpolation = 'Linear'
    channels = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(texture.outputs['Color'], channels.inputs['Color'])
    wall = tree.nodes.get('Principled BSDF').outputs[0]
    shaders = []
    for colour, roughness, metallic in ((colours[2], .28, .65), (colours[1], .16, 0.)):
        shader = tree.nodes.new('ShaderNodeBsdfPrincipled')
        shader.inputs['Base Color'].default_value = (*colour, 1.)
        shader.inputs['Roughness'].default_value = roughness
        shader.inputs['Metallic'].default_value = metallic
        shaders.append(shader.outputs[0])
    amount = tree.nodes.new('ShaderNodeMath')
    amount.operation = 'ADD'
    tree.links.new(channels.outputs[1], amount.inputs[0])
    tree.links.new(channels.outputs[2], amount.inputs[1])
    ratio = tree.nodes.new('ShaderNodeMath')
    ratio.operation = 'DIVIDE'
    tree.links.new(channels.outputs[2], ratio.inputs[0])
    tree.links.new(amount.outputs[0], ratio.inputs[1])
    opening = mix_shaders(tree, shaders[0], shaders[1], ratio.outputs[0])
    combined = mix_shaders(tree, wall, opening, amount.outputs[0])
    tree.links.new(combined, tree.nodes.get('Material Output').inputs['Surface'])
    return result


def photovoltaic(pixels):
    result = material('photovoltaic', (.012, .030, .052), .22, .08)
    tree = result.node_tree
    image = bpy.data.images.new('solar-panel-coverage', width=pixels['size'], height=pixels['size'], float_buffer=True)
    image.colorspace_settings.name = 'Non-Color'
    image.pixels.foreach_set(pixels['rgba'])
    image.update()
    texture = tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    texture.interpolation = 'Linear'
    channels = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(texture.outputs['Color'], channels.inputs['Color'])
    shader = tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (.012, .030, .052, 1.)
    busbar = tree.nodes.new('ShaderNodeBsdfPrincipled')
    busbar.inputs['Base Color'].default_value = (.065, .095, .11, 1.)
    busbar.inputs['Roughness'].default_value = .32
    total = tree.nodes.new('ShaderNodeMath')
    total.operation = 'ADD'
    tree.links.new(channels.outputs[1], total.inputs[0])
    tree.links.new(channels.outputs[2], total.inputs[1])
    ratio = tree.nodes.new('ShaderNodeMath')
    ratio.operation = 'DIVIDE'
    tree.links.new(channels.outputs[1], ratio.inputs[0])
    tree.links.new(total.outputs[0], ratio.inputs[1])
    panel = mix_shaders(tree, shader.outputs[0], busbar.outputs[0], ratio.outputs[0])
    frame = tree.nodes.new('ShaderNodeBsdfPrincipled')
    frame.inputs['Base Color'].default_value = (.14, .15, .16, 1.)
    frame.inputs['Metallic'].default_value = .65
    frame.inputs['Roughness'].default_value = .28
    combined = mix_shaders(tree, panel, frame.outputs[0], channels.outputs[0])
    tree.links.new(combined, tree.nodes.get('Material Output').inputs['Surface'])
    return result


def add_product(product, palette, shell):
    geometry = product['mesh']
    data = bpy.data.meshes.new(product['id'])
    data.from_pydata(geometry['vertices'], [], geometry['faces'])
    data.update()
    obj = bpy.data.objects.new(product['id'], data)
    bpy.context.collection.objects.link(obj)
    obj.color = (*product['wall_linear'], 1.)
    for item in [*palette, *(shell[key] for key in product['pattern_keys'])]:
        data.materials.append(item)
    data.polygons.foreach_set('material_index', geometry['materials'])
    uv = data.uv_layers.new(name='Facade')
    values = [value for face in geometry['faces'] for vertex in face for value in geometry['uv'][vertex]]
    uv.data.foreach_set('uv', values)


def add_reference(path, expected):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=path, forward_axis='NEGATIVE_Z', up_axis='Y')
    objects = [obj for obj in bpy.data.objects if obj not in before and obj.type == 'MESH']
    if not objects:
        raise ValueError('Reference OBJ contained no mesh')
    for obj in objects:
        for polygon in obj.data.polygons:
            polygon.use_smooth = False
    points = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    bounds = [[min(point[i] for point in points) for i in range(3)],
              [max(point[i] for point in points) for i in range(3)]]
    if max(abs(bounds[j][i]-expected[j][i]) for j in range(2) for i in range(3)) > .002:
        raise ValueError('OBJ import and PRT world-coordinate bounds differ')
    return dict(meshes=len(objects), bounds_m=bounds)


def prepare(configuration):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.render.resolution_x, scene.render.resolution_y = configuration['profile']
    scene.render.resolution_percentage = 100
    scene.eevee.taa_render_samples = 48
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    scene.world = bpy.data.worlds.new('World')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes.get('Background').inputs[0].default_value = (.18, .24, .30, 1.)
    scene.world.node_tree.nodes.get('Background').inputs[1].default_value = .6
    colours = [(1., 1., 1.), (.018, .044, .065), (.08, .075, .057), (.16, .18, .18)]
    palette = [material('wall', colours[0], .68, object_colour=True),
               material('glass', colours[1], .16), material('frame', colours[2], .28, .65),
               material('roof', colours[3], .72), material('warm canopy', (.32, .16, .055), .55),
               photovoltaic(configuration['solar_tile']) if configuration.get('solar_tile') else None]
    shell = {style: shell_material(style, pixels, colours) for style, pixels in configuration['tiles'].items()}
    for product in configuration['products']:
        add_product(product, palette, shell)
    constructive = None
    if configuration.get('constructive'):
        from render_constructive import add
        constructive = add(configuration['constructive'],material)
    reference = add_reference(configuration['reference_obj'], configuration['reference_bounds_m']) if configuration.get('reference_obj') else None
    centre = configuration.get('environment_centre',configuration['target'])
    span = configuration['ground_span_m']
    bpy.ops.mesh.primitive_plane_add(size=span, location=(centre[0], centre[1], -.025))
    bpy.context.object.data.materials.append(material('ground', (.24, .265, .24), .88))
    bpy.ops.object.light_add(type='SUN', location=(centre[0], centre[1], 200.))
    sun = bpy.context.object
    sun.rotation_euler = Vector((-.45, -.65, -1.)).to_track_quat('-Z', 'Y').to_euler()
    sun.data.energy, sun.data.angle = 2.5, .06
    sun.data.use_shadow = configuration.get('sun_shadow',True)
    bpy.ops.object.light_add(type='AREA', location=(centre[0]-span*.15, centre[1]-span*.2, span*.3))
    fill = bpy.context.object
    fill.rotation_euler = (Vector(centre)-fill.location).to_track_quat('-Z', 'Y').to_euler()
    fill.data.energy, fill.data.shape, fill.data.size = span*span*3., 'DISK', span*.6
    fill.data.use_shadow = configuration.get('ambient_fill_shadow',True)
    bpy.ops.object.camera_add(location=configuration['eye'])
    camera = bpy.context.object
    view = View(camera.location, Vector(configuration['target']), width=configuration['profile'][0],
                height=configuration['profile'][1], fov_deg=configuration['fov_deg'])
    camera.rotation_euler = Matrix([view.basis[0],view.basis[1],-view.basis[2]]).transposed().to_euler()
    camera.data.lens_unit = 'FOV'
    camera.data.sensor_fit = 'VERTICAL'
    camera.data.lens = camera.data.sensor_height/(2*math.tan(math.radians(configuration['fov_deg'])*.5))
    camera.data.clip_end = 100000.
    scene.camera = camera
    width, height = configuration['profile']
    projection = camera.calc_matrix_camera(bpy.context.evaluated_depsgraph_get(), x=width, y=height)
    rotation = camera.rotation_euler.to_matrix()
    actual_basis = [list(rotation.col[0]),list(rotation.col[1]),list(-rotation.col[2])]
    if max(abs(actual_basis[i][j]-view.basis[i][j]) for i in range(3) for j in range(3)) > 1e-5:
        raise RuntimeError('camera demand and render orientation differ')
    focal = height/(2*math.tan(math.radians(configuration['fov_deg'])*.5))
    if max(abs(projection[0][0]*width*.5-focal), abs(projection[1][1]*height*.5-focal)) > .001:
        raise RuntimeError('camera demand and render projection differ')
    scene.render.filepath = configuration['output']
    inspection = configuration.get('inspection_workbench',False)
    if inspection:
        if configuration['products'] or not constructive:
            raise ValueError('Constructive Workbench inspection requires one checked body mesh')
        scene.render.engine = 'BLENDER_WORKBENCH'
        shading=scene.display.shading
        shading.light='STUDIO'
        shading.color_type='MATERIAL'
        shading.show_shadows=True
        shading.show_cavity=True
        shading.cavity_type='BOTH'
        shading.show_specular_highlight=False
        scene.view_settings.view_transform='Standard'
    return dict(renderer=bpy.app.version_string, engine=scene.render.engine,
                products=len(configuration['products'])+int(constructive is not None), profile=configuration['profile'],
                reference_import=reference,constructive_import=constructive,
                camera_eye=list(camera.location),camera_target=configuration['target'],
                camera_basis=actual_basis,environment_centre=centre,
                ambient_fill_shadow=fill.data.use_shadow,sun_shadow=sun.data.use_shadow,
                inspection_workbench=inspection)


def render(configuration):
    result = prepare(configuration)
    bpy.ops.render.render(write_still=True)
    print(json.dumps(result),flush=True)


if __name__ == '__main__':
    arguments = sys.argv[sys.argv.index('--')+1:]
    render(json.loads(Path(arguments[0]).read_text()))
