"""Group by authored mesh membership or material values; extract filled hole components."""

from dataclasses import dataclass,replace

from .component_geometry import component_shape
from .faces import Face,face_shape
from .metadata import inherit
from .wire_components import WireSet,wire_component_shape


@dataclass(frozen=True)
class GroupRecord:
    members: tuple
    name: str


def material_key(definitions,identifier):
    definition=definitions.get(identifier)
    return ('defined',definition.key()) if definition is not None else ('unresolved-index',identifier)


def element_ids(shape):
    geometry=shape.geometry
    if ((geometry.polygons or len(geometry.fields['faces'])) and (geometry.edges or geometry.points)) or (geometry.edges and geometry.points):
        raise NotImplementedError('Mixed primitive group assembly remains pending')
    if geometry.polygons:return tuple((face.material,face.group) for face in geometry.polygons)
    if len(geometry.fields['faces']):
        material=geometry.fields.get('materials');groups=geometry.fields.get('groups')
        return tuple((int(material[i]) if material is not None else 1,int(groups[i]) if groups is not None else 0) for i in range(len(geometry.fields['faces'])))
    return tuple((element.material,element.group) for element in geometry.edges or geometry.points)


def partitions(shape,kind):
    elements=element_ids(shape);records={};labels={};group_names=dict(shape.geometry.group_names);definitions=dict(shape.geometry.material_defs)
    keys={identifier:material_key(definitions,identifier) for identifier in dict.fromkeys(material for material,_ in elements)} if kind=='m' else {}
    for index,(material,group) in enumerate(elements):
        key=group if kind=='g' else keys[material];records.setdefault(key,[]).append(index)
        labels.setdefault(key,group_names.get(group,'') if kind=='g' else definitions[material].name if material in definitions else '')
    return tuple(GroupRecord(tuple(members),labels[key]) for key,members in records.items())


def source_context(shape):
    from .components import Surface
    if shape.geometry.edges and shape.geometry.points:raise NotImplementedError('Mixed primitive group assembly remains pending')
    if shape.geometry.polygons or len(shape.geometry.fields['faces']):
        if shape.geometry.edges or shape.geometry.points:raise NotImplementedError('Mixed primitive group assembly remains pending')
        return Surface(shape),'f'
    return WireSet(shape,'native'),'wire'


def hole_context(shape):
    from .components import Surface
    surface,kind=source_context(shape)
    if kind!='f':return surface,kind,()
    holes=[]
    for face in surface.faces:
        for number,ring in enumerate(face.holes,1):
            uv=(face.uvs[number][::-1],) if face.uvs else ()
            holes.append(Face(ring[::-1],material=face.material,tags=face.tags,uvs=uv,group=face.group))
    if not holes:return surface,kind,()
    result=inherit(face_shape(surface.mesh['vertices'],holes,shape.scope,allow_warped=True),shape)
    return Surface(result),'f',tuple(GroupRecord((i,),'') for i in range(len(holes)))


class GroupSet:
    def __init__(self,shape,kind):
        self.source=shape;self.kind=kind
        if kind=='h':self.context,self.primitive_kind,self.records=hole_context(shape)
        else:
            self.context,self.primitive_kind=source_context(shape);self.records=partitions(shape,kind)
        owners={member:group for group,record in enumerate(self.records) for member in record.members}
        self.neighbours=tuple(frozenset(owners[other] for member in record.members for other in self.context.neighbours[member]
                                       if other in owners and owners[other]!=group) for group,record in enumerate(self.records))

    def query(self,index):
        record=self.records[index];members=tuple(replace(self.context.query(member),index=index) for member in record.members)
        return replace(members[0],index=index,area=sum(member.area for member in members),members=members)


def group_shape(groups,indices,alignment,info):
    members=tuple(sorted({member for index in indices for member in groups.records[index].members}))
    constructor=component_shape if groups.primitive_kind=='f' else wire_component_shape
    return constructor(groups.context,members,alignment,info)


def names(shape,kind):
    return tuple(record.name for record in partitions(shape,kind))
