"""Deterministically separate conflicting prepared CGA texture names; share encoded bytes."""

import hashlib

from outshine_ref.buildings.modelling import Material,Texture
from outshine_ref.buildings.modelling.material_schema import MAPS
from outshine_ref.buildings.modelling.texture import resources


def bind_resources(bound):
    versions={};original={}
    for binding in bound.material_bindings:
        for texture in binding.resources:
            versions.setdefault(texture.path,set()).add(texture.sha256)
            original.setdefault((texture.path,texture.sha256),texture)
    reserved=set(versions);mapped={};prepared={};origins=[]
    for (path,digest),texture in sorted(original.items()):
        target=path
        if len(versions[path])>1:
            prefix='procedural/boolean/'+digest+'/'+hashlib.sha256(path.encode()).hexdigest()
            target=prefix;index=0
            while target in reserved:
                index+=1;target=prefix+'/'+str(index)
            reserved.add(target)
        mapped[(path,digest)]=target
        prepared[target]=texture if target==path else Texture(target,texture.encoded)
    definitions=[]
    for index,binding in enumerate(bound.material_bindings):
        lookup={texture.path:mapped[(texture.path,texture.sha256)] for texture in binding.resources}
        values=dict(binding.material.attributes)
        for key in MAPS:
            value=values.get(key,'')
            if not value:continue
            if value not in lookup:raise ValueError('Boolean material map lacks a prepared resource: '+key)
            values[key]=lookup[value]
        definitions.append((index,Material(binding.material.name,values)))
        origins.extend((index,texture.path,lookup[texture.path],texture.sha256) for texture in binding.resources)
    return tuple(definitions),resources(tuple(prepared.values())),tuple(origins)
