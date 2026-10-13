"""Compare preserved radiance with a higher-sample same-scene control, without claiming an oracle."""

import argparse
import hashlib
import json
from pathlib import Path

import mitsuba as mi
import numpy as np


def compare(low,high):
    configurations=[json.loads(path.with_suffix('.scene.json').read_text()) for path in (low,high)]
    ignored={'spp','output'}
    if [{key:value for key,value in config.items() if key not in ignored} for config in configurations][0]!={key:value for key,value in configurations[1].items() if key not in ignored}:
        raise ValueError('Denoising comparison changed the scene, camera or lighting')
    if configurations[0]['spp']>=configurations[1]['spp']:raise ValueError('Control must have more samples')
    paths=[low.with_name(low.name+'-raw.pfm'),low.with_name(low.name+'-denoised.pfm'),high.with_name(high.name+'-raw.pfm')]
    arrays=[np.asarray(mi.Bitmap(str(path)),dtype=np.float64) for path in paths]
    if any(value.shape!=(1080,1920,3) or not np.isfinite(value).all() for value in arrays):raise ValueError('Incomplete 1080p comparison buffers')
    raw,filtered,reference=arrays;rows=[]
    for label,value in [('raw',raw),('denoised',filtered)]:
        error=value-reference
        rows.append(dict(method=label,linear_rmse=float(np.sqrt(np.mean(error**2))),linear_absolute_p95=float(np.percentile(abs(error),95)),
            linear_rgb_mean=value.mean((0,1)).tolist(),relative_mean_energy_error=float(abs(value.mean()-reference.mean())/max(reference.mean(),1e-12))))
    return dict(status='pass',low_spp=configurations[0]['spp'],control_spp=configurations[1]['spp'],profile=[1920,1080],metrics=rows,
        input_sha256={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},
        limits='Higher-sample Monte Carlo control remains noisy and shares the seed; spatial error only, no ground-truth, temporal or native-renderer admission')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('low',type=Path);parser.add_argument('high',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=compare(args.low,args.high);args.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
