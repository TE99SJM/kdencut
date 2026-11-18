"""Run inside the production image: require real MLT services and offline STT."""
import os
import json
import shlex
import xml.etree.ElementTree as ET
from pathlib import Path
import subprocess
import tempfile
import wave

os.environ['QT_QPA_PLATFORM'] = 'offscreen'
for kind, service in [('filter','vidstab'), ('filter','rnnoise'), ('filter','opencv.tracker'), ('producer','glaxnimate')]:
    result = subprocess.run(['melt','-query',f'{kind}={service}'], capture_output=True, text=True, check=True)
    assert f'identifier: {service}' in result.stdout + result.stderr, result.stdout + result.stderr

with tempfile.TemporaryDirectory() as root:
    source = Path(root)/'silence.wav'
    with wave.open(str(source), 'wb') as audio:
        audio.setnchannels(1); audio.setsampwidth(2); audio.setframerate(16000)
        audio.writeframes(b'\0' * 32000)
    # Run the same SRT script used by the native Captions action with the bundled
    # model. Silence should produce a valid empty SRT, without downloading.
    env = dict(os.environ, XDG_CACHE_HOME=root, OMP_NUM_THREADS='2', NUMBA_NUM_THREADS='2')
    models = Path(root)/'whisper'; models.mkdir()
    (models/'tiny.pt').symlink_to('/opt/kdencut-models/tiny.pt')
    script = '/usr/share/kdenlive/scripts/whisper/whispertosrt.py'
    subprocess.run(['/opt/kdencut-speech/bin/python3', script, str(source), 'tiny', 'device=cpu', 'fp16=False', 'language=en'], env=env, check=True, timeout=180)
    assert source.with_suffix('.srt').exists(), 'Whisper did not write an SRT'
print('Stabilization, noise reduction, tracking, Lottie and offline captions passed')

# Render the installed social presets through MLT, not only an XML/name check.
profiles = ET.parse('/src/data/profiles.xml')
with tempfile.TemporaryDirectory() as root:
    for platform in ('TikTok', 'Reels', 'Shorts'):
        preset = next(p for p in profiles.iter('profile') if p.get('name') == f'KdenCut {platform} (software)')
        destination = Path(root)/f'{platform}.mp4'
        subprocess.run(['melt', 'color:red', 'out=2', '-consumer', 'avformat:'+str(destination),
                        *shlex.split(preset.get('args')), 'real_time=-1'], check=True, timeout=90,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams', '-of', 'json', str(destination)]))
        video = next(s for s in info['streams'] if s['codec_type'] == 'video')
        assert (video['codec_name'], video['width'], video['height']) == ('h264', 1080, 1920), video
print('All three social presets rendered portrait H.264 through MLT')
