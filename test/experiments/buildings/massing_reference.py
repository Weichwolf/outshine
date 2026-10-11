"""Independent library baseline: union complete closed floor-band prisms in Manifold3D."""

import manifold3d as manifold
import numpy as np

from shell import prepare


def connected_fans(cached, order):
    properties = np.ascontiguousarray(np.c_[cached['vertices'], cached['uv']][cached['faces'][order]].reshape(-1, 5),
                                     dtype=np.float64)
    _, positions = np.unique(properties[:, :3], axis=0, return_inverse=True)
    parent = np.arange(len(properties))

    def root(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    edges = {}
    for first in range(0, len(properties), 3):
        for a, b in ((first, first+1), (first+1, first+2), (first+2, first)):
            edges.setdefault(tuple(sorted((positions[a], positions[b]))), []).append((a, b))
    for group in edges.values():
        if len(group) == 2:
            (a, b), (c, d) = group
            if positions[a] != positions[d] or positions[b] != positions[c]:
                raise ValueError('inconsistent directed prism boundary')
            for start, end in ((a, d), (b, c)):
                start, end = root(start), root(end)
                parent[max(start, end)] = min(start, end)
    targets = np.array([root(i) for i in range(len(parent))], dtype=np.uint64)
    duplicate = np.flatnonzero(targets != np.arange(len(targets))).astype(np.uint64)
    return properties, np.arange(len(properties), dtype=np.uint64).reshape(-1, 3), duplicate, targets[duplicate]


def closed_prism(product, index, material_id):
    layer = product['layers'][index]
    band = dict(layers=[layer], opening_patterns=product['opening_patterns'],
                facades=[f for f in product['facades'] if f['layer'] == index])
    cached, _ = prepare(band)
    faces = cached['faces']
    order = np.argsort(cached['materials'], kind='stable')
    properties, triangles, duplicate, targets = connected_fans(cached, order)
    classes, starts = np.unique(cached['materials'][order], return_index=True)
    runs = np.r_[starts, len(faces)].astype(np.uint64)*3
    face_id = (cached['parts'][order]+len(product['layers'])+1).astype(np.uint64)
    encoded = manifold.Mesh64(properties, triangles, merge_from_vert=duplicate, merge_to_vert=targets,
                              run_index=runs, run_original_id=(classes.astype(np.uint32)+material_id), face_id=face_id)
    solid = manifold.Manifold(encoded)
    if solid.status() != manifold.Error.NoError:
        raise ValueError(f'Manifold rejected floor band: {solid.status()}')
    return solid


def union(product):
    stride = 6+len(product['opening_patterns'])
    material_id = manifold.Manifold.reserve_ids(stride*len(product['layers']))
    solids = [closed_prism(product, index, material_id+index*stride) for index in range(len(product['layers']))]
    combined = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
    encoded = combined.to_mesh64()
    if combined.status() != manifold.Error.NoError:
        raise ValueError(f'Manifold rejected band union: {combined.status()}')
    attributes = np.asarray(encoded.vert_properties)
    faces = np.asarray(encoded.tri_verts, dtype=np.int32)
    materials = np.empty(len(faces), dtype=np.uint8)
    for start, end, original in zip(encoded.run_index[:-1], encoded.run_index[1:], encoded.run_original_id):
        materials[start//3:end//3] = (original-material_id) % stride
    return dict(vertices=attributes[:, :3].copy(), faces=faces.copy(), uv=attributes[:, 3:5].astype(np.float32),
                materials=materials)


def expected_measurements(product):
    layers = product['layers']
    volume = sum(layer['footprint'].area*(layer['top_m']-layer['bottom_m']) for layer in layers)
    area = sum(layer['footprint'].length*(layer['top_m']-layer['bottom_m']) for layer in layers)
    return volume, area+2*layers[0]['footprint'].area


def check_measurements(product, geometry):
    vertices = geometry['vertices']
    triangles = (vertices-vertices.mean(0))[geometry['faces']]
    normals = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
    volume = np.einsum('ij,ij->i', triangles[:, 0], normals).sum()/6
    area = np.linalg.norm(normals, axis=1).sum()*.5
    expected = expected_measurements(product)
    if (not np.isfinite(vertices).all() or np.any(np.linalg.norm(normals, axis=1) <= 1e-12)
            or not np.allclose([volume, area], expected, atol=1e-7, rtol=1e-9)):
        raise AssertionError(f'invalid exterior: {(volume, area)} != {expected}')
    return dict(volume_m3=float(volume), exterior_area_m2=float(area))
