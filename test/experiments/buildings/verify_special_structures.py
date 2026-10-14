"""Provider-limited calibration and cross-site structural checks, with explicit unknowns."""

import argparse
from collections import Counter
from dataclasses import asdict
import hashlib
import json
import math
from pathlib import Path
import resource
import time

import numpy as np
import shapely

from evidence import revision
from special_sources import INVENTORY, SITES, read_site
from outshine_ref.buildings.structures import SPECIAL_KINDS, classify_structure, classify_structure_marker


# Known location/feature witnesses are evaluation labels, never classifier inputs.
CASES = (
    ('Spittelau','building:11451449662','chimney'),
    ('Donauturm','building:2116140522','tower'),
    ('Donauturm','building:2116140532','mast'),
    ('BerlinFernsehturm','building:3978916422','tower'),
    ('BerlinFernsehturm','building:3049871762','mast'),
    ('Drax','building:968472140','cooling_tower'),
    ('Neurath','building:2609491150','cooling_tower'),
    ('Brocken','building:2017362542','tower'),
    ('TokyoSkytree','building:2882691470','tower'),
    ('TokyoSkytree','building:3623514152','tower_section'),
    ('TokyoSkytree','building:2882691482','tower_section'),
    ('TokyoSkytree','building:3623514182','ordinary'),
    ('Grohnde','building:1483486752','chimney'),
    ('Grohnde','building:312561210','cooling_tower'),
    ('Grohnde','building:1061412372','industrial_round_body'),
    ('Grohnde','building:1061412312','ordinary'),
)


def interpret(rows):
    industry_rows = [r for r in rows if r['layer']=='landuse' and r['properties'].get('class')=='industrial']
    industrial = shapely.union_all([r['geometry'] for r in industry_rows])
    decisions, rejected = [], []
    for row in rows:
        if row['layer']!='building':
            continue
        polygon = row['geometry']
        coverage = min(1., polygon.intersection(industrial).area/polygon.area) if polygon.area else 0.
        ids = tuple(r['id'] for r in industry_rows if r['geometry'].intersects(polygon))
        try:
            result = classify_structure(polygon, row['properties'], coordinate_step_m=row['coordinate_step_m'],
                industrial_coverage=coverage, clipped=row['tile_boundary'], evidence_ids=ids)
        except ValueError as error:
            rejected.append(dict(id=row['id'], reason=str(error)))
            continue
        decisions.append((row,result))
    return decisions, rejected


def evaluate(site, decisions):
    result = []
    for name, source, expected in CASES:
        if name!=site:
            continue
        matches = [(row,decision) for row,decision in decisions if row['source_id']==source]
        # A neighbouring tile fragment cannot establish a class; retain it in the survey.
        complete = [(r,d) for r,d in matches if not r['tile_boundary']]
        result.append(dict(source_id=source, expected=expected, instances=len(complete),
            observed=sorted({d.kind for r,d in complete}), status='pass' if complete and all(d.kind==expected for r,d in complete) else 'fail',
            scope='Structural family/interpretation, not proof of a fine OSM type or an exact blueprint'))
    return result


def comparison(features):
    records=[];checks=[]
    for site,source,kind in CASES:
        selected=[f for f in features if f['site']==site and f['source_id']==source and not f['clipped']]
        results=[]
        for f in selected:
            m=f['decision']['metrics'];polygon=shapely.from_wkb(f['polygon_wkb'])
            baseline=m['height_m']>3*math.sqrt(m['area_m2'])
            expected=kind!='ordinary'
            results.append(baseline==expected)
            records.append((polygon,f['source_properties'],dict(coordinate_step_m=m['coordinate_uncertainty_m']/math.sqrt(2),
                industrial_coverage=m['industrial_coverage'],evidence_ids=f['decision']['evidence_ids'])))
        checks.append(dict(site=site,source_id=source,expected_special=kind!='ordinary',
            previous_height_area_gate='pass' if results and all(results) else 'fail'))
    timings={}
    for name in ('height-area-gate','structural-family'):
        samples=[]
        for repeat in range(24):
            started=time.perf_counter()
            for polygon,properties,kwargs in records:
                if name=='height-area-gate':
                    _=(properties.get('render_height',9)-properties.get('render_min_height',0))>3*math.sqrt(polygon.area)
                else:
                    classify_structure(polygon,properties,**kwargs)
            elapsed=(time.perf_counter()-started)*1000
            if repeat>=3:samples.append(elapsed)
        timings[name]=dict(batch_p50_ms=float(np.percentile(samples,50)),batch_p95_ms=float(np.percentile(samples,95)),
            repeats=21,batch_instances=len(records),scope='Warm local Python; predecoded identical footprints/context; no frame timing')
    return dict(binary_checks=checks,previous_gate_passed=sum(c['previous_height_area_gate']=='pass' for c in checks),
        cases=len(checks),timings=timings,
        quality_scope='Binary special-form gate only. Old height/area rule has no specific types; rejected cases are not equal-quality winners.')


def survey(output):
    began=time.perf_counter();sites=[];features=[];markers=[];all_classes=Counter();fields={};timings=[]
    for name,lat,lon,radius,split in SITES:
        rows, source = read_site(name)
        for row in rows:
            props=row['properties'];layer=row['layer'];fields.setdefault(layer,Counter()).update(props.keys())
            if layer=='poi':
                all_classes.update([props.get('subclass',props.get('class',''))])
                marker=classify_structure_marker(props)
                if marker:
                    hosts=[r['id'] for r in rows if r['layer']=='building' and not r['properties'].get('hide_3d')
                           and r['geometry'].covers(row['geometry'])]
                    markers.append(dict(site=name,id=row['id'],source_id=row['source_id'],
                        source_sha256=row['source_sha256'],geometry_wkb=row['geometry'].wkb_hex,source_properties=props,
                        marker=asdict(marker),candidate_hosts=hosts,
                        association='unique geometric host; whole-building type unproven' if len(hosts)==1 else
                            'ambiguous' if hosts else 'standalone',
                        dimensions='No footprint or metric height supplied for point marker'))
        start=time.perf_counter();decisions,rejected=interpret(rows);timings.append(time.perf_counter()-start)
        counts=Counter(d.kind for row,d in decisions);origin=Counter(d.origin for row,d in decisions)
        sites.append(dict(name=name,split=split,counts=dict(counts),origins=dict(origin),
            rows=len(rows),rejected=rejected,source_rejections=source['rejected'], cases=evaluate(name,decisions)))
        for row,d in decisions:
            props=row['properties'].copy();no_name={k:v for k,v in props.items() if not k.startswith('name')}
            repeated=classify_structure(row['geometry'],no_name|{'name':'unrelated landmark'},
                coordinate_step_m=row['coordinate_step_m'],industrial_coverage=d.metrics.industrial_coverage,
                clipped=row['tile_boundary'],evidence_ids=d.evidence_ids)
            if repeated!=d:
                raise ValueError('Classification used names or changed on replay')
            features.append(dict(site=name,id=row['id'],source_id=row['source_id'],source_sha256=row['source_sha256'],
                polygon_wkb=row['geometry'].wkb_hex,source_properties=props,clipped=row['tile_boundary'],decision=asdict(d)))
    cases=[c for site in sites for c in site['cases']]
    report=revision()|dict(provider_inventory_sha256=hashlib.sha256(INVENTORY.read_bytes()).hexdigest(),
        sites=sites,features=features,markers=markers,observed_poi_classes=dict(sorted(all_classes.items())),
        delivered_fields={layer:dict(sorted(c.items())) for layer,c in fields.items()},
        special_marker_classes_observed=sorted(set(all_classes)&set(SPECIAL_KINDS)),
        special_marker_classes_not_observed=sorted(set(SPECIAL_KINDS)-set(all_classes)),
        cases=len(cases),passed=sum(c['status']=='pass' for c in cases),name_invariance='pass',comparison=comparison(features),
        seconds=time.perf_counter()-began,classification_seconds_by_site=timings,
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Pinned preferred provider. Known-location structural checks only; no global/fine-type accuracy claim. '
              'Validation cases were inspected during development; regression checks, not untouched statistical validation. '
              'Unlabelled detections retain alternatives. POI names select no structural family. '
              'No render LOD, native frame, cache or A18 proof.',
        status='pass' if all(c['status']=='pass' for c in cases) else 'fail')
    output.mkdir(parents=True,exist_ok=True)
    (output/'classification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/special-structures'))
    args=parser.parse_args();report=survey(args.output)
    print(json.dumps({k:report[k] for k in ('status','cases','passed','seconds','process_peak_rss_mib','special_marker_classes_observed')}))
    if report['status']!='pass':
        for site in report['sites']:
            for case in site['cases']:
                if case['status']!='pass':print(site['name'],json.dumps(case))
        raise SystemExit(1)
