"""Deterministic original feedback layers. Run with Python, NumPy and SciPy.
No sampled game audio is used by this generator.
"""
from pathlib import Path
import numpy as np
from scipy import signal
from scipy.io import wavfile

RATE = 48000
OUT = Path(__file__).resolve().parents[1] / 'assets/sounds/feedback'
OUT.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(9415)

def noise(t, low, high):
    x = rng.normal(size=len(t))
    return signal.sosfilt(signal.butter(2, [low, high], fs=RATE, btype='bandpass', output='sos'), x)

def tone(t, freq):
    return np.sin(2*np.pi*np.cumsum(np.broadcast_to(freq, t.shape))/RATE)

def save(name, x, loop=False):
    x = np.asarray(x)
    x -= x.mean(axis=0)
    if not loop:
        n = min(150, len(x)//8)
        x[:n] *= np.linspace(0, 1, n)
        x[-1200:] *= np.linspace(1, 0, 1200)
    x *= min(.72/max(np.abs(x).max(), 1e-9), .14/max(np.sqrt(np.mean(x*x)), 1e-9))
    assert np.isfinite(x).all()
    wavfile.write(OUT/name, RATE, (x*32767).astype(np.int16))
    print(name, 'peak', round(float(np.abs(x).max()), 3))

for v in range(3):
    t = np.arange(int(.7*RATE))/RATE
    body = tone(t, 62 + 115*np.exp(-t*36)) * np.exp(-t*14)
    shell = sum(np.sin(2*np.pi*f*(1+v*.015)*t)*np.exp(-t*d) for f,d in [(230,18),(421,25),(713,32)])
    save(f'blunt_{v+1}.wav', .85*body + .12*shell + .65*noise(t,90,1600)*np.exp(-t*65))
    t = np.arange(int(.65*RATE))/RATE
    gate = .18 + .82*(np.sin(2*np.pi*(38+v*3)*t)>.3)
    arc = noise(t,900,6500)*gate*np.exp(-t*9)
    buzz = np.sin(2*np.pi*145*t + 2.6*np.sin(2*np.pi*(820-370*t)*t))*np.exp(-t*12)
    save(f'electric_{v+1}.wav', .9*arc + .2*buzz + .25*noise(t,1200,8000)*np.exp(-t*100))
    t = np.arange(int(.95*RATE))/RATE
    pulse = tone(t, 66 + 150*np.exp(-t*28))*np.exp(-t*10)
    bloom = (1-np.exp(-t*75))*np.exp(-t*5)
    chord = sum(np.sin(2*np.pi*f*t) for f in [523.25,784.88,1046.5])/3
    save(f'core_{v+1}.wav', .75*pulse + .24*chord*bloom + .26*noise(t,600,5000)*np.exp(-t*30))
    t = np.arange(int(.85*RATE))/RATE
    pressure = tone(t, 48+95*np.exp(-t*16))*np.exp(-t*8)
    spool = tone(t, 100+220*(1-np.exp(-t*8)))*(1-np.exp(-t*60))*np.exp(-t*5)
    save(f'ignition_{v+1}.wav', .75*pressure+.25*spool+.4*noise(t,140,2600)*np.exp(-t*8))

# Four seconds of periodic harmonics and FFT-shaped turbulence: no discontinuous loop edit.
t = np.arange(4*RATE)/RATE
freq = np.fft.rfftfreq(len(t), 1/RATE)
z = rng.normal(size=len(freq))+1j*rng.normal(size=len(freq))
z *= (freq>45)*np.exp(-freq/1800)
air = np.fft.irfft(z, n=len(t)); air /= np.std(air)
rotor = sum(np.sin(2*np.pi*f*t)/n for n,f in enumerate([48,96,144,192,288],1))
save('engine_loop.wav', .4*rotor*(.9+.1*np.cos(2*np.pi*12*t))+.16*air, True)
