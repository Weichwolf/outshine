"""Publish checked experiment images to a small, local progress showcase."""

import argparse
from datetime import datetime, timezone
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
from urllib.parse import unquote, urlsplit

from PIL import Image


REPO = Path(__file__).resolve().parents[3]
EXPERIMENTS = REPO/'build/experiments/buildings'
WEBROOT = Path('/tmp/outshine-buildings-showcase')
TITLES = {
    'osm-detail': ('OSM · gelieferter Baukörper', 'quelle', 'Grundriss, Höhenintervall und Quellfarbe; vollständiger Rohling ohne unbelegte Ausstattung.'),
    'bauhaus-detail': ('Bauhaus · Nahdetail', 'nah', 'Räumliche Öffnungen aus dem gemeinsamen Fassadenplan.'),
    'midcentury-detail': ('Mid-century · kleines Haus', 'nah', 'Horizontale Verglasung, flaches Vordach und Dach-PV.'),
    'artdeco-detail': ('Art déco · Nahdetail', 'nah', 'Vertikaler Öffnungsrhythmus auf demselben OSM-Grundriss.'),
    'bauhaus-shell': ('Nahansicht · Hüllenvergleich', 'vergleich', 'Erzwungene grobe Hülle als Vergleich, keine freigegebene Nahstufe.'),
    'bauhaus-reference-distance8.0': ('Ferne · Detailreferenz', 'fern', 'Erzwungenes Nahdetail für den Bildvergleich auf Distanz.'),
    'bauhaus-detail-distance8.0': ('Ferne · gewählte Hülle', 'fern', 'Automatische Detailwahl; Coverage erhält den Öffnungsrhythmus.'),
    'vienna-city-shell': ('Wien · Stadtkörper', 'stadt', 'Echte OSM-Verteilung mit Höfen, grobe Flachdachkörper.'),
    'tokyo-city-shell': ('Tokyo · Stadtkörper', 'stadt', 'Dichte Stichprobe mit gelieferten Höhen und Farben.'),
    'centralpark-city-shell': ('Central Park · Stadtkörper', 'stadt', 'Blick auf die Parkkante, keine vollständige Dichteabnahme.'),
}
REPORT_FIELDS = ('commit', 'experiment_tree_dirty', 'profile', 'triangles', 'openings',
                 'admitted_products', 'source_products', 'mode', 'cache_hit', 'distance',
                 'demand_and_emission_ms', 'source_sha256', 'limitations', 'image_sha256', 'floor_bands', 'planned_features', 'representation', 'recipes',
                 'reference', 'source_id', 'view', 'comparison_case', 'rooms', 'doors', 'generation_ms',
                 'plan_json_bytes', 'raw_mesh_bytes', 'alternatives', 'method', 'plan_status',
                 'stage', 'circulation', 'circulation_status', 'core_alternatives', 'room_kinds',
                 'storeys', 'stair_connections', 'vertical_status', 'checks', 'source_coverage')
REPORT_FIELDS += ('roof_kind','roof_method','roof_rise_m','roof_facets','body_top_m','camera_origin','lod_status')
REPORT_FIELDS += ('component_triangles','completed_recipe_families','glass_alpha')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO, text=True).strip()


def atomic_write(path, data):
    temporary = path.with_suffix(path.suffix+'.tmp')
    temporary.write_bytes(data)
    temporary.replace(path)


def image_record(report_path, root):
    stem = report_path.name.removesuffix('-report.json')
    title, category, caption = TITLES.get(stem, (stem.replace('-', ' '), 'neu', 'Neues Gebäudeexperiment.'))
    if report_path.parent.name == 'cache-hit':
        title, category, caption = 'Cachetreffer · Nahdetail', 'cache', 'Frischer Offline-Prozess; Bild entspricht dem Aufbau aus Quellen.'
    record = dict(id=str(report_path.relative_to(EXPERIMENTS)), title=title, category=category,
                  caption=caption, status='incomplete', updated=report_path.stat().st_mtime)
    try:
        report = json.loads(report_path.read_text())
        if report.get('invalid_reason'):
            raise ValueError(report['invalid_reason'])
        record.update({key: report[key] for key in REPORT_FIELDS if key in report})
        if report.get('representation') == 'functional-plan':
            record.update(title='Gebäudeplan · '+report['label'], category='plan',
                          caption=f"{report['rooms']} generische Räume, {report['doors']} offene Türportale. "
                                  '2D-Grundriss, echter Mesh-Schnitt und 2.5D desselben Erdgeschosses. '
                                  'Raumprogramm, Obergeschosse und native Begehbarkeit sind offen.')
            if report.get('plan_status') == 'unplanned':
                record['caption'] = 'Quelle erhalten; Erdgeschossplan mit konkretem Grund ausgelassen: '+('; '.join(report['limitations']))
            if report.get('stage') == 'circulation':
                record['title'] = 'Flur/Kern · '+report['label']
                if report.get('circulation_status') == 'common-access':
                    core = report['circulation']
                    kind = 'Treppenreserve' if core['core_kind'] == 'stair-reservation' else 'Technikreserve'
                    record['caption'] = (f"{core['private_rooms']} Nutzflächen mit direktem Flurzugang, {kind}, "
                                         f"{report['doors']} echte Türportale. "
                                         f"Flur {core['corridor_area_m2']:.1f} m² ({core['corridor_fraction']:.1%} der Innenfläche). "
                                         'Gleiches Quellgebäude wie im ersten Grundrissversuch. '
                                         'Treppen, Raumprogramm und Obergeschosse bleiben offen.')
            elif report.get('stage') == 'vertical':
                record['title'] = 'Geschosse/Treppen · '+report['label']
                if report.get('vertical_status') == 'connected-storeys':
                    record['caption'] = (f"{report['storeys']} Geschosse, {report['rooms']} logische Flächen und "
                                         f"{report['stair_connections']} konstruierte Verbindungen. "
                                         'Stufen, Podeste, Geländer und echte Deckenöffnungen; Grundriss, überlagerte Mesh-Schnitte und 2.5D. '
                                         'Raumpläne pro Geschoss wiederholt; Nutzung, Außenanschluss und native Spielerphysik bleiben offen.')
                else:
                    record['caption'] = 'Quelle erhalten; Geschossplan mit konkretem Grund ausgelassen: '+('; '.join(report['limitations']))
        elif report.get('representation') == 'roof-plan':
            names = {'flat':'Flach','hip':'Walm','gable':'Sattel mit einem First','shed':'Pult'}
            view = {'plan':'Grundriss / Schnitt / 2.5D','near':'Nahansicht','orbit':'Gegenansicht','far':'Ferne · vollständige Detailreferenz'}[report['view']]
            record.update(title=f"Dach · {report['label']} · {names[report['roof_kind']]} · {view}",category='dach',
                          caption=f"{report['storeys']} generische Geschosse, {report['roof_facets']} Dachflächen; Quelle/Höfe/Gesamthöhe erhalten. "
                                  f"Dach {report['roof_rise_m']:.2f} m innerhalb der Quellhöhe reserviert. "
                                  'Konstruktive Komponenten; noch keine vollständige Architekturfamilie, keine freigegebene Fern-LOD.')
        elif report.get('representation') == 'opening-components':
            view = {'plan':'2D / Schnitt / 2.5D','near':'Nahansicht','orbit':'Gegenansicht',
                    'far':'Distanz · vollständige Referenz','window':'Nahfenster','entrance':'Eingang'}[report['view']]
            record.update(title=f"Öffnungsbauteile · {report['label']} · {view}",category='bauteil',
                          caption=f"{report['openings']} Bauteilplätze an vorhandenen Raum-/Öffnungs-IDs, {report['component_triangles']} zusätzliche Dreiecke. "
                                  'Rahmen, Einfassung, geneigte Bank und offener Eingangsrahmen. '
                                  'Glas als dünne Alpha-Approximation; Räume generisch, Nachtlicht/native LODs offen.')
        elif report.get('representation') == 'esri-reference':
            ref = report['reference']
            label = {'near': 'Nahansicht', 'orbit': 'Rundum-Gegenansicht', 'far': 'Distanzansicht'}[report['view']]
            record.update(title='Esri Candler · '+report['comparison_case']+' · '+label, category='esri',
                          caption=f"Ausführbare Esri-Referenz auf {report['source_id']}, PRT {'.'.join(map(str, ref['prt_version']))}. "
                                  f"Dachabschluss {ref['height_above_provider_m']:.2f} m über dem Höhenparameter. Fremde Referenzregel samt Beispielmaterialien.")
        elif report.get('comparison_case'):
            record.update(title='Eigener Stand · '+report['comparison_case'], category='vergleich',
                          caption=f"Gleicher OSM-Grundriss {report['source_id']}, Kamera und Licht wie die Esri-Nahansicht. Architekturqualität noch unzureichend.")
        elif report.get('representation') == 'context-diagnostic':
            record.update(title='OSM · Nutzungskontext im Grundriss', category='quelle',
                          caption='Diagnosekarte: Farben zeigen abgeleitete Rezeptfamilien; Punkte zeigen lokale POI-Nutzungen. '
                                  'Grundrisse und Höfe kommen unverändert aus dem Provider.')
        elif report.get('representation') == 'provider-context':
            families = sorted({r['family'] for r in report.get('recipes', [])})
            record.update(title='OSM · '+', '.join(families), category='quelle',
                          caption='Gelieferter Grundriss, Höhen und Farbe; Fassadenrhythmus aus gekennzeichneten Nutzungshinweisen. '
                                  'POIs behalten ihren Teil-/Geschossumfang. Keine belegte Fassadenrekonstruktion.')
            if report.get('cache_hit'):
                record.update(title='Cachetreffer · OSM mit Kontext', category='cache')
        elif report.get('representation') == 'provider-volume':
            if report.get('cache_hit'):
                record.update(title='Cachetreffer · OSM-Baukörper', category='cache')
        elif report.get('massing') and any(item['mode'] == 'plain' for item in report['massing']):
            record.update(title=title+' · Grundkörper', category='vergleich',
                          caption='Gleiche OSM-Eingabe ohne prozedurale Staffelungen, PV und Balkone.')
        elif report.get('floor_bands') and not report.get('cache_hit'):
            record['caption'] = ('Gespeicherte Körper-/Fassadenpläne, Dach-/Fassaden-PV und begrenzte Balkone. '
                                 'Bauhaus-Staffelung; Art déco als gezielter Formakzent.') if stem != 'midcentury-detail' else caption
        if stem.startswith(('bauhaus', 'midcentury', 'artdeco')):
            record['caption'] = 'Früherer Stilversuch; gehört nicht zum aktuellen OSM-Rezeptziel. '+record['caption']
        source = report_path.with_name(stem+'.png')
        data = source.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != report['image_sha256']:
            raise ValueError('Bild und Messbericht passen noch nicht zusammen.')
        with Image.open(source) as picture:
            picture.verify()
        with Image.open(source) as picture:
            if list(picture.size) != report['profile']:
                raise ValueError('Bildgröße stimmt nicht mit dem Renderprofil überein.')
        if not report.get('renderer') or not report.get('commit'):
            raise ValueError('Rendernachweis oder Commit fehlt.')
        destination = root/'assets'/f'{stem}-{digest[:16]}.png'
        if not destination.exists():
            atomic_write(destination, data)
        record.update(status='ready', image='assets/'+destination.name)
    except (OSError, ValueError, KeyError, TypeError) as error:
        record['error'] = str(error)
    return record


def benchmark():
    path = EXPERIMENTS/'measurements/benchmark.json'
    if not path.exists():
        return None
    source = json.loads(path.read_text())
    full = {}
    for row in source['measurements']:
        key = (row['site'], row['method'])
        if key not in full or row['requested_source_polygons'] > full[key]['requested_source_polygons']:
            full[key] = row
    fields = ('site', 'method', 'products', 'potential_openings', 'median_generation_ms',
              'repeat_payload_bytes', 'process_peak_rss_bytes', 'common_decode_and_plan_ms')
    return dict(commit=source['commit'], environment=source.get('environment', {}),
                measurements=[{field: row[field] for field in fields} for row in full.values()],
                scope='Repeat-Encoding auf identischen Plänen; Quellen, Pläne und Hüllen separat. '
                      'RSS umfasst den ganzen Python-Prozess. Keine Frame- oder GPU-Zeiten.')


def massing_benchmark():
    path = EXPERIMENTS/'massing-measurements/benchmark.json'
    if not path.exists():
        return None
    source = json.loads(path.read_text())
    full = {}
    for row in source['measurements']:
        key = row['site'], row['method']
        if key not in full or row['requested_source_polygons'] > full[key]['requested_source_polygons']:
            full[key] = row
    fields = ('site', 'method', 'products', 'bands', 'setbacks', 'median_generation_ms', 'process_peak_rss_bytes')
    rows = [dict(**{field: row[field] for field in fields}, repeat_payload_bytes=row['hull_payload_bytes'])
            for row in full.values()]
    return dict(commit=source['commit'], measurements=rows,
                scope='Vollständige Körper gleicher Geschosspläne: direkte Außenflächen gegen Manifold-3D-Union. '
                      'Volumen/Außenfläche geprüft; Quellen/Planung separat. Payload: XYZ, UV, Indices, Material; '
                      'PV/Balkone und Auswahlmetadaten separat. RSS: ganzer Prozess. Keine Frame-/GPU-Zeit.')


def publish(root):
    root.mkdir(parents=True, exist_ok=True)
    (root/'assets').mkdir(exist_ok=True)
    for name in ('showcase.html', 'showcase.css', 'showcase.js'):
        destination = root/('index.html' if name.endswith('.html') else name)
        atomic_write(destination, (Path(__file__).parent/name).read_bytes())
    provider_path = EXPERIMENTS/'provider-comparison.json'
    provider = json.loads(provider_path.read_text()) if provider_path.is_file() else None
    if provider:
        atomic_write(root/'provider-comparison.json', provider_path.read_bytes())
    validation_path = EXPERIMENTS/'provider-validation.json'
    validation = json.loads(validation_path.read_text()) if validation_path.is_file() else None
    context_path = EXPERIMENTS/'context-validation.json'
    context_validation = json.loads(context_path.read_text()) if context_path.is_file() else None
    vertical_path = EXPERIMENTS/'vertical-validation.json'
    vertical_validation = json.loads(vertical_path.read_text()) if vertical_path.is_file() else None
    roof_path = EXPERIMENTS/'roof-validation.json'
    roof_validation = json.loads(roof_path.read_text()) if roof_path.is_file() else None
    esri = {}
    for key, relative in [('catalog', 'esri-catalog.json'), ('validation', 'esri-validation.json'),
                          ('oracle', 'esri-oracle/oracle-summary.json'), ('ghidra', 'esri-ghidra/ghidra-core.json')]:
        path = EXPERIMENTS/relative
        esri[key] = json.loads(path.read_text()) if path.is_file() else None
        if path.is_file():
            atomic_write(root/('esri-'+key+'.json'), path.read_bytes())
    images = [image_record(path, root) for path in EXPERIMENTS.rglob('*-report.json')]
    latest = {}
    for item in sorted(images, key=lambda item: item['updated']):
        key = Path(item['id']).name, item.get('cache_hit', False), item['title'].endswith('Grundkörper')
        latest[key] = item
    images = sorted(latest.values(), key=lambda item: item['updated'], reverse=True)
    commit = git('rev-parse', 'HEAD')
    branch = git('branch', '--show-current')
    manifest = dict(updated=datetime.now(timezone.utc).isoformat(timespec='seconds'), commit=commit,
                    branch=branch, subject=git('log', '-1', '--format=%s'),
                    pushed=git('rev-parse', 'origin/'+branch) == commit,
                    working_tree_dirty=bool(git('status', '--porcelain')),
                    images=images, benchmark=benchmark(), massing_benchmark=massing_benchmark(),
                    provider_comparison=provider, source_validation=validation, context_validation=context_validation,
                    vertical_validation=vertical_validation, roof_validation=roof_validation, esri=esri,
                    progress='Konstruktive Pläne über die ganze Quellhöhe mit direktem Flurzugang, Stufen, Podesten und Geländern. '
                             'Ghidra belegt ausgewählte Esri-Geometriepfade; eigene Architektur und Spielintegration erreichen das Qualitätsziel noch nicht.',
                    gaps=['Nutzungsspezifische Raum-/Geschossprogramme, weitere Treppentypen, Aufzüge und Außenanschlüsse',
                          'Erkennbare Architekturrezepte, Dachformen, Erdgeschosse und konstruktive Details',
                          'Raumausstattung hinter Fenstern, native Begehbarkeit und kohärentes Nachtlicht',
                          'Materialvielfalt und Tausende belegte Varianten eigener Rezeptfamilien',
                          'Weitere ausführbare Esri-Regeln und Abgleich der SDK-/PRT-Versionen',
                          'Infrastrukturkontakte, Verdeckung und Schatten',
                          'Native Weltbudgets und visuelle Abnahme'])
    manifest['revision'] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
    atomic_write(root/'manifest.json', (json.dumps(manifest, ensure_ascii=False, indent=2)+'\n').encode())
    return dict(root=str(root), commit=commit, pushed=manifest['pushed'],
                ready=sum(item['status'] == 'ready' for item in images),
                incomplete=sum(item['status'] != 'ready' for item in images))


class Handler(SimpleHTTPRequestHandler):
    def permitted(self):
        path = Path(unquote(urlsplit(self.path).path).lstrip('/'))
        allowed = str(path) in ('.', 'index.html', 'manifest.json', 'showcase.css', 'showcase.js', 'provider-comparison.json',
                               'esri-catalog.json', 'esri-validation.json', 'esri-oracle.json', 'esri-ghidra.json')
        allowed |= len(path.parts) == 2 and path.parts[0] == 'assets' and path.suffix == '.png'
        return allowed and (Path(self.directory)/path).resolve().is_relative_to(Path(self.directory).resolve())

    def do_GET(self):
        if self.permitted():
            super().do_GET()
        else:
            self.send_error(404)

    def do_HEAD(self):
        if self.permitted():
            super().do_HEAD()
        else:
            self.send_error(404)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def log_message(self, format, *args):
        if len(args) > 1 and str(args[1]) not in ('200', '304'):
            super().log_message(format, *args)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=WEBROOT)
    parser.add_argument('--publish', action='store_true')
    parser.add_argument('--serve', action='store_true')
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8766)
    args = parser.parse_args()
    if args.publish:
        print(json.dumps(publish(args.root), ensure_ascii=False), flush=True)
    if args.serve:
        factory = lambda *a, **kw: Handler(*a, directory=str(args.root.resolve()), **kw)
        with ThreadingHTTPServer((args.host, args.port), factory) as server:
            print(f'Showcase: http://{args.host}:{server.server_port}', flush=True)
            server.serve_forever()
    if not args.publish and not args.serve:
        parser.error('Bitte --publish oder --serve angeben.')


if __name__ == '__main__':
    main()
