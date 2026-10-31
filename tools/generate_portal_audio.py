"""Original portal/checkpoint sounds; optional local Apex blends with provenance.
Run with Python + numpy/scipy, optionally passing the extracted audio folder.
"""
from pathlib import Path
import sys,json,hashlib
import numpy as np
from scipy import signal
from scipy.io import wavfile
root=Path(__file__).resolve().parents[1]/'assets/sounds'
raw=Path(sys.argv[1]) if len(sys.argv)>1 else None
local=root/'local-apex'; output=root/'feedback'
meta=json.loads((local/'SOURCE.json').read_text()) if raw else None
sr=48000; rng=np.random.default_rng(9435)
def sample(pattern,index):
    files=sorted(raw.glob(pattern)); p=files[index%len(files)]; r,x=wavfile.read(p)
    if x.dtype.kind!='f': x=x.astype(float)/32768
    if x.ndim==1: x=np.column_stack([x,x])
    if r!=sr: x=signal.resample_poly(x,sr,r)
    x=signal.sosfilt(signal.butter(2,[100,7000],fs=sr,btype='bandpass',output='sos'),x,axis=0)
    active=np.flatnonzero(np.max(abs(x),axis=1)>.002); x=x[max(0,active[0]-96):]
    return p,x*.35/max(np.max(abs(x)),1e-9)
def write(p,x):
    x-=x.mean(axis=0); x[:96]*=np.linspace(0,1,96)[:,None]
    n=min(2400,len(x)//4); x[-n:]*=np.linspace(1,0,n)[:,None]
    x*=min(.60/max(abs(x).max(),1e-9),.10/max(np.sqrt(np.mean(x*x)),1e-9))
    wavfile.write(p,sr,(x*32767).astype(np.int16))
for kind,count,duration,pattern in [('transit',3,1.05,'Alter_Tac_Portal_Enter_1p_2ch_V6_*'),('portal_open',3,.65,'Alter_Tac_Portal_Activate_3p_2ch_V11_*'),('checkpoint_charge',1,.24,'coop_generator_shieldrecharge_start_2ch_v2*'),('checkpoint_ready',1,.9,'coop_generator_shieldrecharge_end_2ch_v1*')]:
    for i in range(count):
        t=np.arange(int(sr*duration))/sr
        noise=signal.sosfilt(signal.butter(2,[180,4500],fs=sr,btype='bandpass',output='sos'),rng.normal(size=len(t)))
        if kind=='checkpoint_charge':
            env=np.sin(np.pi*t/duration)**.7
            mono=(.18*np.sin(2*np.pi*np.cumsum(160+950*(t/duration)**2)/sr)+.12*noise)*env
        elif kind=='checkpoint_ready':
            mono=.18*np.sin(2*np.pi*95*t)*np.exp(-t*18)
            for start,hz in [(0,660),(.10,990),(.20,1320)]:
                u=np.maximum(t-start,0); mono+=.12*np.sin(2*np.pi*hz*u)*np.exp(-u*9)*(t>=start)
        else:
            env=(1-np.exp(-t*60))*np.exp(-t*5)
            mono=(.16*np.sin(2*np.pi*np.cumsum(120+1200*np.exp(-t*12))/sr)+.25*noise)*env
        x=np.column_stack([mono,mono])
        if kind=='transit':
            # A bounded stereo sweep plus delayed decaying arrival, not a gameplay delay.
            sweep=np.linspace(-.6,.6,len(x)); x[:,0]*=1-sweep; x[:,1]*=1+sweep
            delay=int(.18*sr); x[delay:]+=.28*x[:-delay].copy()[:,::-1]
        write(output/f'{kind}_{i+1}.wav',x.copy())
        if raw:
            p,s=sample(pattern,i); n=min(len(s),len(x)); s=s[:n].copy(); fade=min(2400,n//4); s[-fade:]*=np.linspace(1,0,fade)[:,None]; x[:n]+=.7*s
            sources=[p]
            if kind=='transit':
                q,e=sample('Alter_Tac_Portal_Exit_3p_2ch_v3_*',i); start=int(.22*sr); n=min(len(e),len(x)-start); e=e[:n].copy(); e[-2400:]*=np.linspace(1,0,2400)[:,None]; x[start:start+n]+=.55*e; sources.append(q)
            name=f'{kind}_0{i+1}.wav'; write(local/name,x)
            meta['files']=[e for e in meta['files'] if e['file']!=name]
            meta['files'].append(dict(file=name,sources=[dict(name=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sources],processing='Trimmed, filtered, layered with original synthesis and short fades; transit entry/exit crossfade',seconds=duration))
if raw: (local/'SOURCE.json').write_text(json.dumps(meta,indent=2),encoding='utf-8')
print('Generated 8 original cues and optional local variants.')
