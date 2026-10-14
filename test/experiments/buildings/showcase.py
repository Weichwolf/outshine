"""Publish four verified complete tile images with manual refresh."""

import argparse
from datetime import datetime,timezone
import hashlib
from http.server import SimpleHTTPRequestHandler,ThreadingHTTPServer
import json
import shutil
from pathlib import Path
import subprocess
from urllib.parse import unquote,urlsplit

from PIL import Image
from showcase_profile import PROFILE


REPO=Path(__file__).resolve().parents[3]
EXPERIMENTS=REPO/'build/experiments/buildings'
WEBROOT=EXPERIMENTS/'showcase/site'



def git(*args):
    return subprocess.check_output(['git',*args],cwd=REPO,text=True).strip()


def atomic_write(path,data):
    temporary=path.with_suffix(path.suffix+'.tmp')
    temporary.write_bytes(data);temporary.replace(path)


def image_record(path,root):
    record=dict(id=path.name,status='incomplete')
    try:
        report=json.loads(path.read_text());record.update({key:report[key] for key in ('title','caption','profile','commit','view')})
        if report.get('view')!='tile' or report.get('representation')!='showcase-current':
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
            if any(actual<minimum for actual,minimum in zip(picture.size,PROFILE,strict=True)):
                raise ValueError('Detailprüfung benötigt mindestens 1920×1080 Pixel.')
        if not report.get('renderer') or not report.get('commit') or report.get('experiment_tree_dirty'):
            raise ValueError('Sauberer Rendernachweis mit Commit fehlt.')
        name=source.stem+'-'+digest[:16]+'.png';destination=root/'assets'/name
        if not destination.exists():atomic_write(destination,data)
        record.update(status='ready',image='/assets/'+name)
    except (OSError,ValueError,KeyError,TypeError) as error:
        record.update(status='incomplete',error=str(error))
    return record


def current_images(root):
    folder=EXPERIMENTS/'tile-showcase';path=folder/'reports.json'
    if not path.is_file():return [],None
    snapshot=json.loads(path.read_text());images=[]
    for name in snapshot['reports']:
        if Path(name).name!=name or not name.endswith('-report.json'):
            raise ValueError('Snapshot contains a report outside its output directory')
        record=image_record(folder/name,root)
        expected=snapshot['commit']
        if record.get('commit')!=expected:
            record.pop('image',None)
            record.update(status='incomplete',error='Bildreport stammt aus einem anderen Snapshot.')
        images.append(record)
    return images,snapshot


def publish(root):
    snapshot=json.loads((EXPERIMENTS/'tile-showcase/reports.json').read_text())
    if len(snapshot['reports'])!=4 or len(set(snapshot['reports']))!=4:
        raise ValueError('Current showcase requires exactly four tile reports')
    for name in snapshot['reports']:
        if Path(name).name!=name or not name.endswith('-report.json'):
            raise ValueError('Snapshot contains a report outside its output directory')
        report=json.loads((EXPERIMENTS/'tile-showcase'/name).read_text())
        if report.get('view')!='tile':raise ValueError('Showcase requires whole-tile images')
        source=report['source']
        if source['generated']+len(source['omissions'])!=source['delivered_building_polygons']:
            raise ValueError('Tile source accounting incomplete')
        if report['renderer']['included_source_triangles']!=report['triangles']:
            raise ValueError('Tile renderer omitted generated triangles')
    root.mkdir(parents=True,exist_ok=True);(root/'assets').mkdir(exist_ok=True)
    images,snapshot=current_images(root)
    if any(row['status']!='ready' for row in images):raise ValueError('Four current tile images must be verified')
    for name in ('showcase.css','showcase.js'):
        atomic_write(root/name,Path(__file__).with_name(name).read_bytes())
    atomic_write(root/'index.html',Path(__file__).with_name('showcase.html').read_bytes())
    commit=git('rev-parse','HEAD');branch=git('branch','--show-current')
    manifest=dict(updated=datetime.now(timezone.utc).isoformat(timespec='seconds'),commit=commit,branch=branch,
        render_commit=snapshot['commit'],pushed=git('rev-parse','origin/'+branch)==commit,
        working_tree_dirty=bool(git('status','--porcelain')),images=images)
    atomic_write(root/'manifest.json',(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode())
    used={Path(row['image']).name for row in images};removed=0
    for asset in (root/'assets').glob('*.png'):
        if asset.name not in used:asset.unlink();removed+=1
    for folder in root.iterdir():
        if folder.is_dir() and folder.name!='assets':shutil.rmtree(folder)
    return dict(root=str(root),commit=commit,pushed=manifest['pushed'],removed_obsolete_assets=removed,ready=4,incomplete=0)


class Handler(SimpleHTTPRequestHandler):
    def permitted(self):
        path=Path(unquote(urlsplit(self.path).path).lstrip('/'))
        allowed=str(path) in ('.','index.html','manifest.json','showcase.css','showcase.js')
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
