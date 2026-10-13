"""Replace the showcase snapshot with fresh interior, building, group and provider-city views."""

import argparse
import hashlib
import json
from pathlib import Path

from evidence import revision
from opening_probe import scene
from preview import configuration,run_render
from showcase_scene import LABELS,cached,context_scene,fitted
from showcase_profile import PROFILE
from showcase_split_plan import probe as split_plan_probe
from showcase_split import probe as split_probe,open_probe
from showcase_area_split import probe as area_probe
from showcase_insert import probe as insert_probe
from showcase_asset_load import probe as import_probe
from showcase_uv import probe as uv_probe
from showcase_physical import probe as physical_probe
from projection_render import probe as projection_probe
from projection_axes_render import probe as projection_axes_probe
from showcase_setbacks import probe as setbacks_probe
from showcase_setback_physical import probe as setback_physical_probe
from showcase_offset import probe as offset_probe
from showcase_roof_shed import probe as roof_shed_probe


NAMES={'rectangle':'Rechteck · Kontrollhaus','vienna-compact':'Wien · kompaktes Gebäude',
       'vienna-courtyard':'Wien · Hofgebäude','tokyo-compact':'Tokyo · kompaktes Gebäude'}


def render(config,output,stem,report):
    config.update(profile=PROFILE,inspection_workbench=True)
    image=output/(stem+'.png');config['output']=str(image.resolve())
    result=run_render(config,output/(stem+'-scene.json'))
    record=report|result|dict(representation='showcase-current',profile=PROFILE,
                            image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                            completed_recipe_families=0,lod_status='inspection reference; native LOD not admitted')
    path=output/(stem+'-report.json')
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(page=record['page'],title=record['title'],seconds=result['render_seconds'])),flush=True)
    return path.name


def building_views(body,plans,output,provenance):
    label=body['label'];plan=body['plan'];rows=[]
    base=provenance|dict(label=label,source_id=plan['id'],triangles=len(body['mesh']['faces']),
                         raw_mesh_bytes=sum(array.nbytes for array in body['mesh'].values()),
                         rooms=sum(len(ids) for ids in plan['storey_room_ids']),
                         doors=sum(len(portals) for portals in plan['storey_portals']),
                         storeys=plan['storeys'],openings=len(body['slots']),
                         cache_source_commit=body['report']['commit'],source_plan_sha256=body['plan_sha256'],
                         geometry_scope='complete cached roof/body, stairs and opening components')
    for view in ('near','orbit','window','entrance'):
        reference=plans/f'roof-{label}-flat-{view if view in ("near","orbit") else "near"}-scene.json'
        stem=f'current-{label}-{view}'
        config=scene(reference,output/(stem+'.png'),body['mesh_path'],view,body['slots'])
        interior=view in ('window','entrance')
        if interior:
            config['constructive']['glass_part_ids']=[]
        caption=('Blick durch die tatsächliche Fensteröffnung.' if view=='window' else 'Offenes Eingangsportal und Innenanschluss.') if interior else 'Vollständiges gespeichertes Gebäude mit Geschossen, Treppen und Öffnungsbauteilen.'
        caption+=' Generische Räume, Workbench-Studiolicht; '+('Glas zur Geometriekontrolle ausgeblendet.' if interior else 'Glas als opake Prüfoberfläche.')
        title=NAMES[label]+' · '+dict(near='Gesamtansicht',orbit='Gegenansicht',window='Fenster und Raum',entrance='Eingang und Flur')[view]
        rows.append(render(config,output,stem,base|dict(page='interiors' if interior else 'buildings',view=view,title=title,caption=caption)))
    return rows


def urban_views(site,bodies,output,provenance,group):
    mesh,path,data=context_scene(site,bodies,output,group)
    page='groups' if group else 'cities'
    rows=[]
    for view in ('near','orbit') if group else ('overview',):
        camera=fitted(mesh,PROFILE,opposite=view=='orbit',city=not group)
        stem='current-'+site.lower()+'-'+page+'-'+view
        config,_=configuration([],camera,False,output/(stem+'.png'))
        config['constructive']=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                    wall_linear=[1.,1.,1.],roof_linear=[.155,.179,.196],extra_wall_colours=data['extra_wall_colours'])
        counts={key:data[key] for key in ('source_products','admitted_products','constructive_products',
                'source_volume_products','source_sha256','triangles','raw_mesh_bytes','generation_ms','tile_boundary_products')}
        count=data['constructive_products'];coarse=data['source_volume_products']
        caption=f'{count} aktuelle konstruktive Prüfgebäude und {coarse} einfache Quellkörper an echten OSM-Positionen. '
        caption+='Grundrisse, Höfe, Höhen und gelieferte Wandfarben erhalten. '
        caption+='Straßen/Gelände und vollständige Architektur der Nachbarn fehlen; Workbench-Studiolicht.'
        title=('Wien' if site=='Vienna' else 'Central Park' if site=='CentralPark' else site)+' · '+('Gebäudegruppe' if group else 'Stadtübersicht')
        if view=='orbit':title+=' · Gegenansicht'
        rows.append(render(config,output,stem,provenance|counts|dict(page=page,view=view,title=title,caption=caption,
                     source_metadata=path.stem+'-sources.json',geometry_scope='current focal meshes and provider-volume context; no invented architecture copies')))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path('build/experiments/buildings/opening-inspections'))
    parser.add_argument('--plans',type=Path,default=Path('build/experiments/buildings/roof-plans'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/current-showcase'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    provenance=revision();bodies=[cached(label,args.source,args.plans) for label in LABELS]
    reports=[report for body in bodies for report in building_views(body,args.plans,args.output,provenance)]
    for site in ('Vienna','Tokyo','CentralPark'):
        focal=[body for body in bodies if body['plan']['id'].startswith(site+':')]
        if focal:reports+=urban_views(site,focal,args.output,provenance,True)
        reports+=urban_views(site,focal,args.output,provenance,False)
    reports+=split_plan_probe(args.output,provenance)
    reports+=split_probe(args.output,provenance)
    reports+=open_probe(args.output,provenance)
    reports+=area_probe(args.output,provenance)
    reports+=insert_probe(args.output,provenance)
    reports+=import_probe(args.output,provenance)
    reports+=uv_probe(args.output,provenance)
    reports+=physical_probe(args.output,provenance)
    reports+=projection_probe(args.output,provenance)
    reports+=projection_axes_probe(args.output,provenance)
    reports+=setbacks_probe(args.output,provenance,render)
    reports+=setback_physical_probe(args.output,provenance)
    reports+=offset_probe(args.output,provenance)
    reports+=roof_shed_probe(args.output,provenance)
    temporary=args.output/'snapshot.json.tmp'
    temporary.write_text(json.dumps(provenance|dict(reports=reports,completed_recipe_families=0),ensure_ascii=False,indent=2)+'\n')
    temporary.replace(args.output/'snapshot.json')


if __name__=='__main__':
    main()
