"""Publish one checked current snapshot and serve four building view scales locally."""

import argparse
from datetime import datetime,timezone
import hashlib
from http.server import SimpleHTTPRequestHandler,ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
from urllib.parse import unquote,urlsplit

from PIL import Image


REPO=Path(__file__).resolve().parents[3]
EXPERIMENTS=REPO/'build/experiments/buildings'
WEBROOT=EXPERIMENTS/'showcase/site'
PAGES={
    'interiors':dict(path='innenraeume',title='Innenräume',description='Fenster, Eingänge und die Räume dahinter.',
                     scope='Höchster Detailbedarf: räumliche Öffnungen, Raumanschlüsse und Wege.',
                     gap='Räume sind noch generisch und leer. Ausstattung, Beleuchtung und native Begehbarkeit fehlen.'),
    'buildings':dict(path='gebaeude',title='Einzelne Gebäude',description='Ganze Gebäude aus mehreren Blickrichtungen.',
                     scope='Nahbereich: vollständige Körper, Geschosse, Treppen und Öffnungsbauteile.',
                     gap='Nutzungsspezifische Fassaden, Erdgeschosse und vollständige Architekturrezepte fehlen.'),
    'groups':dict(path='gebaeudegruppen',title='Gebäudegruppen',description='Aktuelle Gebäude im Zusammenhang mit ihren echten Nachbarn.',
                  scope='Mittlere Ansicht: Abstände, Nachbarschaft, Höfe und Verdeckung.',
                  gap='Die meisten Nachbarn sind einfache Quellkörper. Gemeinsame Fassadenkontakte, Straßen und Gelände fehlen.'),
    'cities':dict(path='staedte',title='Städte',description='OSM-Verteilung und Silhouetten im Stadtmaßstab.',
                  scope='Große Ansicht: komplette gepinnte Stadtkacheln, keine erfundene Vervielfältigung.',
                  gap='Stadtübersichten zeigen überwiegend Quellkörper; vollständige Stadtarchitektur und native Fern-LODs fehlen.'),
}


def git(*args):
    return subprocess.check_output(['git',*args],cwd=REPO,text=True).strip()


def atomic_write(path,data):
    temporary=path.with_suffix(path.suffix+'.tmp')
    temporary.write_bytes(data);temporary.replace(path)


def image_record(path,root):
    record=dict(id=path.name,status='incomplete')
    try:
        report=json.loads(path.read_text());record.update(report)
        if report.get('page') not in PAGES or report.get('representation')!='showcase-current':
            raise ValueError('Report gehört nicht zum aktuellen Showcase-Snapshot.')
        if report.get('invalid_reason'):
            raise ValueError(report['invalid_reason'])
        source=path.with_name(path.name.removesuffix('-report.json')+'.png')
        data=source.read_bytes();digest=hashlib.sha256(data).hexdigest()
        if digest!=report['image_sha256']:
            raise ValueError('Bild und Report stimmen nicht überein.')
        with Image.open(source) as picture:
            picture.verify()
        with Image.open(source) as picture:
            if list(picture.size)!=report['profile']:
                raise ValueError('PNG-Größe und Renderprofil stimmen nicht überein.')
        if not report.get('renderer') or not report.get('commit') or report.get('experiment_tree_dirty'):
            raise ValueError('Sauberer Rendernachweis mit Commit fehlt.')
        name=source.stem+'-'+digest[:16]+'.png';destination=root/'assets'/name
        if not destination.exists():atomic_write(destination,data)
        record.update(status='ready',image='/assets/'+name)
    except (OSError,ValueError,KeyError,TypeError) as error:
        record.update(status='incomplete',error=str(error))
    return record


def current_images(root):
    folder=EXPERIMENTS/'current-showcase';path=folder/'snapshot.json'
    if not path.is_file():return [],None
    snapshot=json.loads(path.read_text());images=[]
    for name in snapshot['reports']:
        if Path(name).name!=name or not name.endswith('-report.json'):
            raise ValueError('Snapshot contains a report outside its output directory')
        record=image_record(folder/name,root)
        if record.get('commit')!=snapshot['commit']:
            record.pop('image',None)
            record.update(status='incomplete',error='Bildreport stammt aus einem anderen Snapshot.')
        images.append(record)
    return images,snapshot


def publish(root):
    root.mkdir(parents=True,exist_ok=True);(root/'assets').mkdir(exist_ok=True)
    template=Path(__file__).with_name('showcase.html').read_bytes()
    atomic_write(root/'index.html',template)
    for page in PAGES.values():
        directory=root/page['path'];directory.mkdir(exist_ok=True)
        atomic_write(directory/'index.html',template)
    for name in ('showcase.css','showcase.js'):
        atomic_write(root/name,Path(__file__).with_name(name).read_bytes())
    images,snapshot=current_images(root)
    commit=git('rev-parse','HEAD');branch=git('branch','--show-current')
    ghidra_path=EXPERIMENTS/'esri-ghidra/ghidra-core.json'
    ghidra=json.loads(ghidra_path.read_text()) if ghidra_path.is_file() else None
    frontend=hashlib.sha256(b''.join((root/name).read_bytes() for name in ('index.html','showcase.css','showcase.js'))).hexdigest()
    manifest=dict(updated=datetime.now(timezone.utc).isoformat(timespec='seconds'),commit=commit,branch=branch,frontend_sha256=frontend,
                  pushed=git('rev-parse','origin/'+branch)==commit,working_tree_dirty=bool(git('status','--porcelain')),
                  render_commit=snapshot['commit'] if snapshot else None,pages=PAGES,images=images,
                  completed_recipe_families=snapshot.get('completed_recipe_families',0) if snapshot else 0,
                  progress='Zuerst der vollständige Python-Modellierkern nach CityEngine, danach moderne/zeitgenössische Architekturrezepte für alle OSM-Klassen und gültigen Kombinationen. '
                           'Aktuell bestehen konstruktive Prüfgebäude mit Räumen, Geschossen, Treppen und Öffnungen. '
                           'Die Stadtumgebung besteht überwiegend aus einfachen Quellkörpern.',
                  lod_note='Die vier Bereiche sind Ansichtsebenen. Gemessene LOD-Grenzen und native Echtzeitabnahme stehen noch aus.',
                  ghidra=dict(functions=len(ghidra['selected_functions']),sdk=ghidra['sdk_version'],
                              version=ghidra['ghidra_version'],commit=ghidra['commit']) if ghidra else None)
    manifest['revision']=hashlib.sha256(json.dumps(manifest,sort_keys=True).encode()).hexdigest()
    atomic_write(root/'manifest.json',(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode())
    used={Path(row['image']).name for row in images if row['status']=='ready'};removed=0
    for asset in (root/'assets').glob('*.png'):
        if asset.name not in used:asset.unlink();removed+=1
    return dict(root=str(root),commit=commit,pushed=manifest['pushed'],removed_obsolete_assets=removed,
                ready=sum(row['status']=='ready' for row in images),
                incomplete=sum(row['status']!='ready' for row in images))


class Handler(SimpleHTTPRequestHandler):
    def permitted(self):
        path=Path(unquote(urlsplit(self.path).path).lstrip('/'))
        pages={page['path'] for page in PAGES.values()}
        allowed=str(path) in ('.','index.html','manifest.json','showcase.css','showcase.js')
        allowed|=str(path) in pages or str(path) in {page+'/index.html' for page in pages}
        allowed|=len(path.parts)==2 and path.parts[0]=='assets' and path.suffix=='.png'
        return allowed and (Path(self.directory)/path).resolve().is_relative_to(Path(self.directory).resolve())

    def do_GET(self):
        if self.permitted():super().do_GET()
        else:self.send_error(404)

    def do_HEAD(self):
        if self.permitted():super().do_HEAD()
        else:self.send_error(404)

    def end_headers(self):
        self.send_header('Cache-Control','no-store');super().end_headers()

    def log_message(self,format,*args):
        if len(args)>1 and str(args[1]) not in ('200','304'):super().log_message(format,*args)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=WEBROOT)
    parser.add_argument('--publish',action='store_true');parser.add_argument('--serve',action='store_true')
    parser.add_argument('--host',default='127.0.0.1');parser.add_argument('--port',type=int,default=8766)
    args=parser.parse_args()
    if args.publish:print(json.dumps(publish(args.root),ensure_ascii=False),flush=True)
    if args.serve:
        factory=lambda *a,**kw:Handler(*a,directory=str(args.root.resolve()),**kw)
        with ThreadingHTTPServer((args.host,args.port),factory) as server:
            print(f'Showcase: http://{args.host}:{server.server_port}',flush=True);server.serve_forever()
    if not args.publish and not args.serve:parser.error('Bitte --publish oder --serve angeben.')


if __name__=='__main__':main()
