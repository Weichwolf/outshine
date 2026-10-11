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
               (.41,.30,.18),(.50,.41,.30),(.09,.12,.13),(.35,.35,.35),configuration['roof_linear']]
    mesh = bpy.data.meshes.new('constructive-source-mesh')
    mesh.from_pydata(data['vertices'].tolist(),[],data['faces'].tolist())
    mesh.update()
    for index,colour in enumerate(palette):
        mesh.materials.append(material('constructive-'+str(index),colour,.16 if index==3 else .72))
    mesh.polygons.foreach_set('material_index',data['materials'])
    obj = bpy.data.objects.new('constructive-building',mesh)
    bpy.context.collection.objects.link(obj)
    referenced = data['vertices'][np.unique(data['faces'])]
    return dict(triangles=len(data['faces']),vertices=len(data['vertices']),
                mesh_sha256=configuration['mesh_sha256'],bounds_m=[referenced.min(axis=0).tolist(),referenced.max(axis=0).tolist()])
