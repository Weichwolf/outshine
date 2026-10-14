"""Bind CSG triangle ancestry to resolved materials, original groups and source tags."""

from dataclasses import dataclass

import numpy as np

from outshine_ref.buildings.modelling.material import resolved_materials

BOOL_AUTO_TAGS=frozenset({'bool.A','bool.B','bool.cut'})

@dataclass(frozen=True)
class MaterialBinding:
    material: object
    resources: tuple
    source_materials: tuple


@dataclass(frozen=True)
class BooleanAttributes:
    materials: object
    groups: object
    source_faces: object
    tags: tuple
    material_bindings: tuple
    group_names: tuple
    group_origins: tuple


def immutable(values,dtype):
    raw=np.asarray(values,dtype=dtype)
    return np.frombuffer(raw.tobytes(),dtype=raw.dtype).reshape(raw.shape)


def bind_attributes(product,max_materials=65536):
    if type(max_materials)is not int or not 0<max_materials<=65536:
        raise ValueError('Boolean material capacity must fit uint16')
    count=len(product.mesh['faces'])
    if product.operands.shape!=(count,) or product.source_triangles.shape!=(count,):
        raise ValueError('Boolean attribute ancestry does not cover output triangles')
    if product.operands.dtype.kind not in 'iu':raise ValueError('Boolean operand identities must be integer indices')
    if np.any((product.operands<0)|(product.operands>=len(product.sources))):
        raise ValueError('Boolean attribute ancestry has invalid operands')
    materials=np.empty(count,dtype=np.uint16);groups=np.empty(count,dtype=np.uint32)
    parents=np.empty(count,dtype=np.int32);tags=[None]*count
    bindings=[];material_keys={};origins=[];group_names=[];used_groups=set();next_group=0
    for owner,source in enumerate(product.sources):
        selected=np.flatnonzero(product.operands==owner)
        indices=product.source_triangles[selected]
        if indices.dtype.kind not in 'iu' or np.any(indices<0) or np.any(indices>=len(source.mesh['faces'])):
            raise ValueError('Boolean attribute relation has invalid source triangle indices')
        definitions=resolved_materials(source.shape)
        source_parts=source.mesh['parts'][indices]
        resources=tuple(source.shape.textures)
        resource_key=tuple((value.path,value.sha256) for value in resources)
        material_ids={};group_ids={};names=dict(source.shape.geometry.group_names)
        for part in np.unique(source_parts):
            face=source.shape.geometry.polygons[int(part)]
            if face.material not in material_ids:
                material=definitions[face.material];key=(material.key(),resource_key)
                if key not in material_keys:
                    if len(bindings)>=max_materials:raise ValueError('Boolean material capacity exceeded')
                    material_keys[key]=len(bindings);bindings.append([material,resources,[]])
                identifier=material_keys[key];bindings[identifier][2].append((owner,face.material))
                material_ids[face.material]=identifier
            if face.group not in group_ids:
                identifier=face.group
                if identifier in used_groups:
                    while next_group in used_groups:next_group+=1
                    identifier=next_group
                if identifier>4294967295:raise ValueError('Boolean group capacity exceeded')
                used_groups.add(identifier);group_ids[face.group]=identifier
                origins.append((identifier,owner,face.group))
                if face.group in names:group_names.append((identifier,names[face.group]))
            rows=selected[source_parts==part]
            materials[rows]=material_ids[face.material];groups[rows]=group_ids[face.group];parents[rows]=part
            captured=(face.tags-BOOL_AUTO_TAGS)|{'bool.A' if owner==0 else 'bool.B'}
            for row in rows:tags[int(row)]=captured
    return BooleanAttributes(immutable(materials,np.uint16),immutable(groups,np.uint32),immutable(parents,np.int32),
        tuple(tags),tuple(MaterialBinding(material,resources,tuple(source_ids)) for material,resources,source_ids in bindings),
        tuple(sorted(group_names)),tuple(sorted(origins)))
