"""Generate original robot layers and optional local-only Apex variants.
Usage: python tools/generate_robot_audio.py [path-to-extracted-audio]
Requires numpy/scipy; the game only needs the generated WAV files.
"""
from pathlib import Path
import sys, json, hashlib
import numpy as np
from scipy import signal
from scipy.io import wavfile
root=Path(__file__).resolve().parents[1]/'assets/sounds'
out=root/'feedback'; out.mkdir(exist_ok=True)
local=root/'local-apex'
raw=Path(sys.argv[1]) if len(sys.argv)>1 else None
rng=np.random.default_rng(9425); sr=48000
manifest=json.loads((local/'SOURCE.json').read_text()) if raw else None
def write(path,x):
    x=x-x.mean(); n=min(240,len(x)//8); x[:n]*=np.linspace(0,1,n)
    x[-2400:]*=np.linspace(1,0,2400) if len(x)>2400 else 1
    x*=min(.62/max(abs(x).max(),1e-9),.105/max(np.sqrt(np.mean(x*x)),1e-9))
    wavfile.write(path,sr,(x*32767).astype(np.int16))
def source(pattern,index):
    files=sorted(raw.glob(pattern)); p=files[index%len(files)]
    r,x=wavfile.read(p)
    if x.dtype.kind!='f': x=x.astype(float)/32768
    if x.ndim==2: x=x.mean(axis=1)
    if r!=sr: x=signal.resample_poly(x,sr,r)
    x=signal.sosfilt(signal.butter(2,[100,5500],fs=sr,btype='bandpass',output='sos'),x)
    active=np.flatnonzero(abs(x)>.002); x=x[max(0,active[0]-96):]
    x*=min(4,.4/max(abs(x).max(),1e-9))
    return p,x
for kind,count,duration in [('shutdown',3,1.7),('powerdown',3,1.5),('boot',3,1.45),('servo',6,.28)]:
    for i in range(count):
        t=np.arange(int(duration*sr))/sr
        noise=signal.sosfilt(signal.butter(2,[200,5000],fs=sr,btype='bandpass',output='sos'),rng.normal(size=len(t)))
        if kind in ('shutdown','powerdown'):
            f=40+600*np.exp(-t*3.5)
            x=.24*np.sin(2*np.pi*np.cumsum(f)/sr)*np.exp(-t*2.8)
            x+=.16*noise*np.exp(-t*6)*(np.sin(2*np.pi*23*t)>0)
            if kind=='shutdown': x+=.6*np.sin(2*np.pi*(63*t+.7*(1-np.exp(-t*25))))*np.exp(-t*16)+.3*noise*np.exp(-t*25)
            pattern='AI_Stalker_RebootPowerDown_1ch_v1_*'; offset=.18 if kind=='shutdown' else 0
        elif kind=='boot':
            f=80+560*(1-np.exp(-t*2.3))
            x=.19*np.sin(2*np.pi*np.cumsum(f)/sr)*np.sin(np.pi*np.minimum(t/1.05,1))**2
            x+=.12*noise*np.exp(-t*8)
            for at,hz in [(.91,660),(1.04,990)]:
                u=np.maximum(0,t-at); x+=.16*np.sin(2*np.pi*hz*u)*np.exp(-u*23)*(t>=at)
            pattern='Pathfinder_Servo_Heavy_Short_1ch_v1_*'; offset=.22
        else:
            f=260+75*i+230*np.sin(np.pi*t/duration)
            env=np.sin(np.pi*t/duration)**2
            x=(.16*np.sin(2*np.pi*np.cumsum(f)/sr)+.06*noise)*env
            pattern='AI_Tick_Mvmt_Servo_Walk_1ch_v1_*'; offset=0
        write(out/f'{kind}_{i+1}.wav',x.copy())
        if raw:
            p,s=source(pattern,i); at=int(offset*sr); n=min(len(s),len(x)-at)
            s=s[:n].copy(); s[:96]*=np.linspace(0,1,96); fade=min(2400,n//3); s[-fade:]*=np.linspace(1,0,fade)
            x[at:at+n]+=.7*s
            name=f'{kind}_0{i+1}.wav'; write(local/name,x)
            manifest['files']=[e for e in manifest['files'] if e['file']!=name]
            manifest['files'].append(dict(file=name,source=p.name,source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),processing='Trimmed, filtered, gain-matched; layered with original sequenced motor/electrical synthesis.',seconds=duration))
if raw: (local/'SOURCE.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('Generated 15 portable layers'+(' and 15 local Apex variants.' if raw else '.'))
