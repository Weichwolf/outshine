"""Import a checked constructive mesh into the shared Eevee comparison scene."""

import hashlib
from pathlib import Path

import bpy
import numpy as np


def add(configuration, material):
    path = Path(configuration['mesh'])
    if hashlib.sha256(path.read_bytes()).hexdigest()!=configuration['mesh_sha256']:
        raise ValueError('Constructive scene mesh differs from its checked payload')
    data = np.load(path)
    palette = [(.56,.48,.36),configuration['wall_linear'],(.72,.73,.74),(.018,.044,.065),(.34,.38,.34),
               (.41,.30,.18),(.50,.41,.30),(.09,.12,.13),(.35,.35,.35),configuration['roof_linear'],(.16,.19,.21),(.43,.47,.49)]
    mesh = bpy.data.meshes.new('constructive-source-mesh')
    mesh.from_pydata(data['vertices'].tolist(),[],data['faces'].tolist())
    mesh.update()
    for index,colour in enumerate(palette):
        shader = material('constructive-'+str(index),colour,.16 if index==3 else .72)
        if index==3 and 'glass_alpha' in configuration:
            alpha = float(configuration['glass_alpha'])
            if not np.isfinite(alpha) or not 0.<alpha<=1.:
                raise ValueError('Window surface alpha must be finite and in (0,1]')
            shader.node_tree.nodes.get('Principled BSDF').inputs['Alpha'].default_value = alpha
            method = configuration.get('glass_method','DITHERED')
            if method not in ('DITHERED','BLENDED'):
                raise ValueError('Unknown window surface transparency method')
            shader.surface_render_method = method
            shader.use_backface_culling = True
            shader.use_transparent_shadow = True
        mesh.materials.append(shader)
    mesh.polygons.foreach_set('material_index',data['materials'])
    obj = bpy.data.objects.new('constructive-building',mesh)
    bpy.context.collection.objects.link(obj)
    referenced = data['vertices'][np.unique(data['faces'])]
    return dict(triangles=len(data['faces']),vertices=len(data['vertices']),
                mesh_sha256=configuration['mesh_sha256'],bounds_m=[referenced.min(axis=0).tolist(),referenced.max(axis=0).tolist()],
                glass_surface='thin alpha approximation' if 'glass_alpha' in configuration else 'opaque reference',
                glass_alpha=configuration.get('glass_alpha',1.),glass_method=configuration.get('glass_method','DITHERED'))
