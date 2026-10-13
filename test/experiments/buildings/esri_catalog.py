"""Inventory local CGA documentation and inspect external Esri rule packages."""

import argparse
from collections import Counter
import hashlib
from html.parser import HTMLParser
import importlib.metadata
import json
from pathlib import Path
import subprocess

from evidence import revision


SDK = Path.home()/'Downloads/esri_ce_sdk-3.4.12206'
EXAMPLES = Path.home()/'Git/pyprt-examples'
EXAMPLES_COMMIT = 'c9ebe92f27f6355179c9d7792f430a3c133b9214'
GROUPS = {
    'Dachformen': 'roofGable roofHip roofPyramid roofRidge roofShed',
    'Massierung und Grundriss': 'extrude envelope footprint innerRect innerRectangle offset '
                             'setback setbackPerEdge setbackToArea splitAndSetbackPerimeter taper',
    'Fassaden und Unterteilung': 'comp split splitArea trim',
    'Geometriebearbeitung': 'boolOp cleanupGeometry convexify deleteHoles inline modify rectify reduceGeometry resetGeometry',
    'Bauteile und Instanzen': 'i insertAlongUV primitive scatter',
    'Transformation und Scope': 'alignScopeToAxes alignScopeToGeometry alignScopeToGeometryBBox center mirror '
                                'mirrorScope pushpop r rotate rotateScope s setPivot shapeLUO t translate',
    'Material und UV': 'color copyUV deleteUV normalizeUV projectUV resetMaterial rotateUV scaleUV setMaterial '
                       'setupProjection texture tileUV translateUV',
    'Normalen und Semantik': 'deleteTags label reverseNormals setNormals setTagsFromEdgeAttrs softenNormals tag',
    'Regelsteuerung und Diagnose': 'NIL print report set',
}
ALIASES = {'rotate': 'r', 'translate': 't', 'innerRect': 'innerRectangle'}
OWN = {
    'i':dict(module='modelling_shape.Shape.insert',status='prepared assets shared across placements; three modes, yUp/zUp, null scopes; OBJ/glTF/GLB static surface import with hierarchy, UV0/UV1, normals, exact material values and encoded images',
             pending=['other file formats and required glTF extensions','animations/skins/morphs/tangents/custom channels','extra-channel geometry editing','texture/PBR rendering','numericalCleanup','trim planes','full current-rule material overrides','isolated PRT insert rule','native GPU instance encoding']),
    'split':dict(module='modelling_shape.Shape.split',status='open surfaces and closed Cartesian solids; native triangle property clipping, closed-only one-sided GEOS caps, logical holes and source appearance ownership; width/area plan prototypes',
                 pending=['mixed open/closed or wire/surface primitive cuts','raw part provenance without logical faces','nested pattern allocation','UV-space/unit-space overloads','SDK cut-face UV/material semantics','isolated PRT split rule']),
    'splitArea':dict(module='modelling_shape.Shape.split_area',status='planar and almost-planar source surface cuts; exact actual triangle areas, logical face plane admission, source appearance and adjusted/signed scopes',
                     pending=['nested pattern allocation','exact SDK plane/projection and UV reconstruction','isolated PRT splitArea rule']),
    'comp': dict(module='modelling_shape.Shape.comp',status='f/e/fe/v/fv/g/m/h components; authored groups, declared material values, filled holes, any-member keywords and inherited frames/attributes; analytic controls and PRT prism bounds/volumes',
                 pending=['mixed primitive group assembly','full shader defaults and texture/object-pointer material equivalence','roof-specific edge selectors','streetWidth maps','trim planes','fractional indices','isolated PRT comp rules','exact nonplanar SDK alignment','degenerate-normal SDK fallback equivalence']),
    't': dict(module='modelling_shape.Shape.t',status='documented forms implemented; scope checks and two PRT translations',pending=[]),
    'r': dict(module='modelling_shape.Shape.r',status='documented pivot/centre forms implemented; scope checks and Ghidra',pending=['isolated PRT rotation rule']),
    's': dict(module='modelling_shape.Shape.s',status='absolute/relative and negative sizes implemented; scope checks and Ghidra',pending=['singular authored-normal transform','isolated PRT scale rule']),
    'translate': dict(module='modelling_shape.Shape.translate',status='documented coordinate systems implemented; scope checks',pending=[]),
    'extrude': dict(module='modelling_shape.Shape.extrude',status='four face and edge extrusion modes; shared endpoint sweeps, holes, UV/tag/material checks, PRT prisms and actual PRT wall surfaces',
                    pending=['mixed primitive extrusion','cancelled-normal SDK fallback and degenerate pruning equivalence','tangential or collapsed face sweep','UV channels beyond the first','isolated PRT edge/flat-top/vertex-normal rules']),
}
OWN_ATTRIBUTES={'comp':dict(module='modelling_shape.CompInfo',status='per-selector sel/index/total assigned; scope operations and extrusion inherit predecessor values',pending=['inheritance in remaining geometry operations']),
                'split':dict(module='modelling_split.SplitInfo',status='global index/total, pattern label, interval, source face ownership and coplanar omissions; inherited by scope, comp and extrude',pending=['nested pattern and UV-space metadata'])}
OWN.update({operation:dict(module='modelling_shape.Shape.'+method,
    status='immutable surface UV channels 0-9; native streams and logical face/hole corners; analytic values and Workbench texture bindings',
    pending=['isolated PRT UV rules','wire and point UV editing','full shader and projection integration'])
    for operation,method in [('scaleUV','scale_uv'),('rotateUV','rotate_uv'),('translateUV','translate_uv'),
                             ('copyUV','copy_uv'),('deleteUV','delete_uv'),('normalizeUV','normalize_uv')]})
OWN_FUNCTIONS={'geometry':dict(module='modelling_shape.Shape.group_names/material_names',status='group/material name queries implemented in component index order without vertex emission; remaining geometry functions pending',
                               pending=['other geometry member functions','full shader defaults and texture/object-pointer material equivalence','isolated PRT name-order rule'])}
OWN.update({operation:dict(module='modelling_shape.Shape.'+method,
    status='immutable shape overrides and geometry definitions; documented precedence, sRGB colours, three reset selectors, SDK and targeted Ghidra evidence',
    pending=['isolated PRT material rules','complete texture-resource binding','exact SDK formatting and shader equivalence'])
    for operation,method in [('color','color'),('setMaterial','set_material'),('resetMaterial','reset_material')]})
OWN_FUNCTIONS['getMaterial']=dict(module='modelling_shape.Shape.get_material',
    status='used/shape/geometry and changed/all; explicit defaults and immutable imported values retained',
    pending=['exact SDK value formatting','isolated PRT selector rules','complete texture-resource binding'])
OWN['texture']=dict(module='modelling_shape.Shape.texture',
    status='encoded image resources retained across shape operations; alpha-aware map assignment; procedural colour/roughness/normal panels rendered on a control house',
    pending=['all SDK image formats','all map transforms and shader bindings','isolated PRT texture rule','filtered runtime LOD'],
    evidence=['verify_modelling_texture.py','verify_procedural_surface.py','verify_physical_binding.py'])
OWN['setupProjection']=dict(module='modelling_shape.Shape.setup_projection',
    status='six scope and six world axes; captured affine frames, signed absolute/relative/floating sizes and offsets; static SDK matrix evidence and analytic controls',
    pending=['large-coordinate SDK origin rebasing','implicit default projection','isolated PRT setup rule'],
    evidence=['verify_modelling_projection.py','esri_projection_reference.py','projection_render.py','projection_axes_render.py'])
OWN['projectUV']=dict(module='modelling_shape.Shape.project_uv',
    status='explicit stored projection baked into channels 0..9 and logical face corners; component/split inheritance and instance minimum dimensions retained',
    pending=['implicit default projection','wire and point primitives','isolated PRT projectUV rule','native runtime LOD'],
    evidence=['verify_modelling_projection.py','projection_render.py','projection_axes_render.py'])
USES = {
    'Dachformen': 'Dachsilhouette, First, Traufe und Überstand; zunächst keine eigene Umsetzung.',
    'Massierung und Grundriss': 'Höhenintervalle, Höfe, Gebäudeteile, Rücksprünge und technische Körper.',
    'Fassaden und Unterteilung': 'Geschosse, Erdgeschoss, Achsen, Öffnungen, Kontaktwände und Giebel.',
    'Geometriebearbeitung': 'Geometrisch gültige Anschlüsse, Durchgänge und komplexe Körper.',
    'Bauteile und Instanzen': 'Fenster, Türen, Tore, Gesimse und konstruktive Wiederholungen.',
    'Transformation und Scope': 'Metrische lokale Bauteilsysteme und konsistente Koordinaten.',
    'Material und UV': 'Quellfarbe, Dach/Wand/Glas, Materialvariation und metrische Texturkoordinaten.',
    'Normalen und Semantik': 'Bauteilzuordnung, Flächennormalen und zusammenhängende Detailprodukte.',
    'Regelsteuerung und Diagnose': 'Deterministische Pläne, begrenzte Expansion und verwertbare Fehler.',
}


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.stack, self.title, self.signatures = [], [], []

    def handle_starttag(self, tag, attrs):
        if tag not in ('meta', 'link', 'img', 'br', 'hr', 'input'):
            classes = dict(attrs).get('class', '').split()
            self.stack.append((tag, classes))

    def handle_endtag(self, tag):
        for i in range(len(self.stack)-1, -1, -1):
            if self.stack[i][0] == tag:
                del self.stack[i:]
                break

    def handle_data(self, data):
        if any(tag == 'title' for tag, _ in self.stack):
            self.title.append(data)
        if any('signatures' in classes for _, classes in self.stack):
            self.signatures.append(data)


def page_record(path, root, kind):
    page = Page()
    data = path.read_bytes()
    page.feed(data.decode('utf-8'))
    identifier = path.stem.removeprefix({'operation': 'op_', 'function': 'func_', 'attribute': 'attr_'}.get(kind, ''))
    own={'operation':OWN,'attribute':OWN_ATTRIBUTES,'function':OWN_FUNCTIONS}.get(kind,{}).get(identifier)
    group = next((group for group, names in GROUPS.items() if identifier in names.split()), None)
    return dict(name=identifier, kind=kind, title=' '.join(''.join(page.title).split()),
                signatures=' '.join(' '.join(page.signatures).split()),
                group=group, alias_of=ALIASES.get(identifier),
                document=str(path.relative_to(root)), sha256=hashlib.sha256(data).hexdigest(),
                status=own['status'] if own else 'documented; individual operation not yet execution-tested',own_python=own)


def packages(root):
    import pyprt
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    if actual != EXAMPLES_COMMIT:
        raise ValueError('Esri example revision differs from the reviewed reference')
    rows = []
    for path in sorted((root/'data').glob('*.rpk')):
        attrs = pyprt.get_rpk_attributes_info(str(path))
        rows.append(dict(name=path.name, bytes=path.stat().st_size,
                         sha256=hashlib.sha256(path.read_bytes()).hexdigest(), attributes=attrs,
                         status='rule metadata inspected; geometry execution is recorded separately'))
    return dict(repository='https://github.com/Esri/pyprt-examples', commit=actual, path=str(root),
                packages=rows, pyprt=importlib.metadata.version('pyprt'), prt_version=pyprt.get_api_version())


def catalog(sdk=SDK, examples=EXAMPLES):
    root = sdk/'doc/html/cgaref/cgareference'
    pages = []
    for prefix, kind in (('op_', 'operation'), ('func_', 'function'), ('attr_', 'attribute')):
        pages.extend(page_record(path, root, kind) for path in sorted(root.glob(prefix+'*.html')))
    pages.extend(page_record(path, root, 'library-function') for path in sorted((root/'celib').glob('*.html')))
    unknown = [p['name'] for p in pages if p['kind'] == 'operation' and not p['group']]
    if unknown:
        raise ValueError(f'Unclassified CGA operations: {unknown}')
    binary = sdk/'bin/libcom.esri.prt.core.so'
    symbols = subprocess.check_output(['nm', '-D', '--demangle', str(binary)], text=True)
    kernels = [line.strip() for line in symbols.splitlines() if 'CoreToolbox::PolygonOps::compute' in line]
    grouped = [dict(name=group, operations=[p['name'] for p in pages if p['group'] == group],
                    building_use=USES[group]) for group in GROUPS]
    return dict(**revision(), sdk_path=str(sdk), sdk_version=[3, 4, 12206],
                sdk_core_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                counts=dict(Counter(p['kind'] for p in pages)), groups=grouped, pages=pages,
                core_kernel_symbols=kernels, references=packages(examples),
                scope='Local SDK documentation and supplied example packages; no claim to catalog all CityEngine content.',
                distinctions='Documented operations, executable reference rules, own recipe families, variants and instances are separate.',
                gaps=['Most CGA operations lack isolated executable cases.',
                      'Bundled PyPRT uses PRT 3.3; SDK 3.4 extensions require matching bindings.',
                      'Own OSM architecture remains below the Esri reference quality.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=SDK)
    parser.add_argument('--examples', type=Path, default=EXAMPLES)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/esri-catalog.json'))
    args = parser.parse_args()
    result = catalog(args.sdk, args.examples)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(output=str(args.output), counts=result['counts'],
                          packages=len(result['references']['packages']), runtime=result['references']['prt_version'])))


if __name__ == '__main__':
    main()
