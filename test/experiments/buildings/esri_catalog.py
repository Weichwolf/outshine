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
    'comp': dict(module='modelling_shape.Shape.comp',status='f/e/fe/v/fv primitives, selectors, exclusion, operators, frames and predecessor attributes implemented; analytic controls and PRT prism bounds/roundtrips',
                 pending=['group/material/hole components','roof-specific edge selectors','streetWidth maps','trim planes','fractional indices','isolated PRT comp rule','exact nonplanar SDK alignment','degenerate-normal SDK fallback equivalence']),
    't': dict(module='modelling_shape.Shape.t',status='documented forms implemented; scope checks and two PRT translations',pending=[]),
    'r': dict(module='modelling_shape.Shape.r',status='documented pivot/centre forms implemented; scope checks and Ghidra',pending=['isolated PRT rotation rule']),
    's': dict(module='modelling_shape.Shape.s',status='absolute/relative and negative sizes implemented; scope checks and Ghidra',pending=['singular authored-normal transform','isolated PRT scale rule']),
    'translate': dict(module='modelling_shape.Shape.translate',status='documented coordinate systems implemented; scope checks',pending=[]),
    'extrude': dict(module='modelling_shape.Shape.extrude',status='four face extrusion modes implemented; holes, connected/disconnected faces, UV/tag/material checks and PRT prisms',
                    pending=['edge extrusion from comp(e/fe)','tangential or collapsed face sweep','UV channels beyond the first','isolated PRT flat-top and vertex-normal rule']),
}
OWN_ATTRIBUTES={'comp':dict(module='modelling_shape.CompInfo',status='per-selector sel/index/total assigned; scope operations and extrusion inherit predecessor values',pending=['inheritance in remaining geometry operations'])}
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
    own=OWN.get(identifier) if kind=='operation' else OWN_ATTRIBUTES.get(identifier) if kind=='attribute' else None
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
