"""Prepared immutable texture bytes shared by shapes and assets, independent of source files."""

from dataclasses import dataclass,replace
from functools import cached_property
import hashlib
from io import BytesIO
from pathlib import Path

from PIL import Image


@dataclass(frozen=True)
class Texture:
    path: str
    encoded: bytes

    def __post_init__(self):
        if not isinstance(self.path,str) or not self.path or not isinstance(self.encoded,bytes):
            raise ValueError('Texture requires a source name and immutable encoded bytes')
        with Image.open(BytesIO(self.encoded)) as image:
            if image.width<1 or image.height<1:raise ValueError('Empty texture')
            image.verify()

    @classmethod
    def from_file(cls,path):
        path=Path(path).resolve();return cls(str(path),path.read_bytes())

    @cached_property
    def metadata(self):
        with Image.open(BytesIO(self.encoded)) as image:
            return image.size,('A' in image.getbands() or 'transparency' in image.info),image.format

    @property
    def has_alpha(self):return self.metadata[1]

    @cached_property
    def sha256(self):return hashlib.sha256(self.encoded).hexdigest()


def resources(*groups):
    result={}
    for group in groups:
        for value in group:
            if not isinstance(value,Texture):raise ValueError('Shape texture resource is not prepared')
            if value.path in result and result[value.path].encoded!=value.encoded:
                raise ValueError('Conflicting texture bytes for the same material path: '+value.path)
            result[value.path]=value
    return tuple(result[key] for key in sorted(result))


def texture(shape,value):
    prepared=value if isinstance(value,Texture) else Texture.from_file(value)
    captured=resources(shape.textures,(prepared,))
    changed=shape.set_material({'colormap':prepared.path,'opacitymap':prepared.path if prepared.has_alpha else ''})
    return replace(changed,textures=captured)
