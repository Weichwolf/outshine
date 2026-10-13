"""Retain encoded material images and sampler links independently of source geometry buffers."""

import base64
from dataclasses import dataclass
import json


@dataclass(frozen=True)
class ImageResource:
    name: str
    mime_type: str
    encoded: bytes


@dataclass(frozen=True)
class Resources:
    images: tuple=()
    textures: tuple=()
    samplers: tuple=()


def uri_bytes(uri,resolver):
    if uri.startswith('data:'):
        prefix,payload=uri.split(',',1)
        if not prefix.endswith(';base64'):raise NotImplementedError('Non-base64 data URI')
        return base64.b64decode(payload,validate=True)
    return resolver.get(uri)


def gltf_resources(header,blob,resolver):
    images=[];buffers={}
    for index,image in enumerate(header.get('images',())):
        if image.get('uri'):encoded=uri_bytes(image['uri'],resolver)
        else:
            view=header['bufferViews'][image['bufferView']];buffer_index=view['buffer']
            if buffer_index not in buffers:
                uri=header['buffers'][buffer_index].get('uri');buffers[buffer_index]=uri_bytes(uri,resolver) if uri else blob
            start=view.get('byteOffset',0);encoded=buffers[buffer_index][start:start+view['byteLength']]
            if len(encoded)!=view['byteLength']:raise ValueError('Material image buffer is truncated')
        images.append(ImageResource(image.get('name',str(index)),image.get('mimeType',''),encoded))
    textures=tuple((texture.get('source'),texture.get('sampler')) for texture in header.get('textures',()))
    samplers=tuple(json.dumps(value,sort_keys=True,separators=(',',':')) for value in header.get('samplers',()))
    for source,sampler in textures:
        if source is not None and (not isinstance(source,int) or not 0<=source<len(images)):
            raise ValueError('Texture references an unknown image')
        if sampler is not None and (not isinstance(sampler,int) or not 0<=sampler<len(samplers)):
            raise ValueError('Texture references an unknown sampler')
    return Resources(tuple(images),textures,samplers)
