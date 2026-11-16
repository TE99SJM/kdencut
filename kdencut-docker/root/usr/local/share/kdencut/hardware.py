"""Probe real encoding capability as the editor user, not just codec names."""
from pathlib import Path
import subprocess


def works(args):
    try:
        return subprocess.run(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20).returncode == 0
    except (OSError, subprocess.TimeoutExpired): return False


def detect(mode='auto', nodes=None, probe=works):
    nodes = sorted(Path('/dev/dri').glob('renderD*')) if nodes is None else nodes
    base = ['ffmpeg', '-v', 'error', '-nostdin']
    sample = ['-f','lavfi','-i','color=size=128x128:rate=30','-frames:v','1','-an']
    if mode in ('auto','nvidia') and probe(base + sample + ['-pix_fmt','yuv420p','-c:v','h264_nvenc','-f','null','-']):
        return {'encoder':'h264_nvenc','decoder':'cuda','device':'0','backend':'nvidia'}
    if mode in ('auto','vaapi'):
        for node in nodes:
            if probe(base + ['-vaapi_device',str(node)] + sample + ['-vf','format=nv12,hwupload','-c:v','h264_vaapi','-f','null','-']):
                return {'encoder':'h264_vaapi','decoder':'vaapi','device':str(node),'backend':'vaapi'}
    return {'encoder':'libx264','decoder':'','device':'','backend':'software'}


def environment(info):
    return {'KDENCUT_RUNTIME':'1', 'KDENCUT_PREVIEW_DECODER':info['decoder'], 'KDENCUT_EXPORT_ENCODER':info['encoder'],
            'MLT_AVFORMAT_HWACCEL':info['decoder'], 'MLT_AVFORMAT_HWACCEL_DEVICE':info['device']}
