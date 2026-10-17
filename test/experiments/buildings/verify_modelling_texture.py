"""Check captured texture ownership and CGA alpha-map replacement through modelling operations."""

import argparse
from io import BytesIO
import json
from pathlib import Path
from tempfile import TemporaryDirectory

from PIL import Image

from evidence import revision
from outshine_ref.buildings.modelling.insert import Asset
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.split_sizes import Part,Floating
from outshine_ref.buildings.modelling.texture import Texture,resources
from modelling_uv_cases import adjacent,courtyard


def prepared(mode,name):
    image=Image.new(mode,(8,4))
    if mode=='P':image.info['transparency']=0
    stream=BytesIO();image.save(stream,format='PNG')
    return Texture(name,stream.getvalue())


def controls():
    rgb=prepared('RGB','opaque');rgba=prepared('RGBA','alpha');palette=prepared('P','palette')
    assert not rgb.has_alpha and rgba.has_alpha and palette.has_alpha
    source=adjacent().delete_uv(0);textured=source.texture(rgba)
    assert textured.geometry is source.geometry and 'uv' not in textured.emit()
    values=dict(textured.get_material('shape','changed'))
    assert values['colormap']=='alpha' and values['opacitymap']=='alpha'
    opaque=textured.texture(rgb);values=dict(opaque.get_material('shape','all'))
    assert values['colormap']=='opaque' and values['opacitymap']==''
    assert source.textures==() and opaque.textures==resources((rgba,rgb))
    children=list(opaque.comp('f'))+list(opaque.split('x',Part(Floating(1)),Part(Floating(1))))
    for child in children:
        assert child.textures==opaque.textures
        assert all(any(value is parent for parent in opaque.textures) for value in child.textures)
    assert courtyard().texture(palette).extrude(3.).textures==(palette,)
    asset=Asset.from_shape(opaque)
    direct=Shape(asset.geometry,opaque.scope,asset=asset)
    assert direct.textures==asset.textures and Asset.from_shape(direct).textures==asset.textures
    placed=source.insert(asset);assert placed.textures==asset.textures
    with TemporaryDirectory(prefix='outshine-texture-',dir='/tmp') as directory:
        path=Path(directory)/'source.png';path.write_bytes(rgb.encoded)
        captured=source.texture(path);path.unlink()
        assert captured.textures[0].encoded==rgb.encoded and captured.textures[0].metadata==rgb.metadata
    conflict=Texture(rgb.path,rgba.encoded)
    for call in (lambda:resources((rgb,conflict)),lambda:source.texture(rgb).texture(conflict),
                 lambda:resources((object(),)),lambda:Texture('',rgb.encoded)):
        try:call()
        except ValueError:pass
        else:raise AssertionError('Invalid texture resource accepted')
    return dict(alpha_detection=True,opacity_replacement=True,no_implicit_uv=True,
                immutable_shared_ownership=True,comp_split_extrude=True,asset_roundtrip=True,
                source_file_independent=True,conflicts_rejected=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if not args.output.resolve().is_relative_to(Path('/tmp').resolve()):
        parser.error('--output must be inside /tmp')
    record=revision()|dict(status='pass',controls=controls(),
        limits='Resource capture only; procedural production recipes, projection, renderer binding and LOD filtering remain open.')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record['controls']))


if __name__=='__main__':main()
