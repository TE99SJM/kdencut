#!/usr/bin/env python3
"""Generate an optional Compose override, without changing compose.yaml or .env."""
import argparse
import json
from pathlib import Path
import subprocess


def command(args):
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=15)
        return result.stdout.strip() if result.returncode == 0 else ''
    except (OSError, subprocess.TimeoutExpired): return ''


def configuration(mode='auto', dri=Path('/dev/dri'), run=command):
    nodes = sorted(dri.glob('renderD*')) if dri.is_dir() else []
    nvidia = bool(run(['nvidia-smi', '--query-gpu=uuid', '--format=csv,noheader'])) if mode in ('auto', 'nvidia') else False
    try: runtimes = json.loads(run(['docker', 'info', '--format', '{{json .Runtimes}}']) or '{}')
    except ValueError: runtimes = {}
    service = {'environment': {'KDENCUT_GPU':'software'}}
    selected = 'software'
    if mode in ('auto', 'nvidia') and nvidia and 'nvidia' in runtimes:
        selected = 'nvidia'
        service['runtime'] = 'nvidia'
        service['deploy'] = {'resources':{'reservations':{'devices':[{'driver':'nvidia','count':'all','capabilities':['gpu']}]}}}
        service['environment'].update(NVIDIA_VISIBLE_DEVICES='all', NVIDIA_DRIVER_CAPABILITIES='compute,video,utility,graphics,display')
        if nodes: service['devices'] = [f'{dri}:/dev/dri']
    elif mode in ('auto', 'vaapi') and nodes:
        selected = 'vaapi'
        service['devices'] = [f'{dri}:/dev/dri']
        node = '/dev/dri/' + nodes[0].name
        service['environment'].update(DRINODE=node, DRI_NODE=node)
    service['environment']['KDENCUT_GPU'] = selected
    return {'services':{'kdencut':service}}, selected


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['auto','software','vaapi','nvidia'], default='auto')
    parser.add_argument('--output', type=Path, default=Path('compose.gpu.generated.yaml'))
    args = parser.parse_args()
    config, selected = configuration(args.mode)
    args.output.write_text(json.dumps(config, indent=2)+'\n')  # JSON is valid YAML.
    print(f'GPU configuration: {selected}; saved {args.output}. Ports, volumes and .env are unchanged.')
    if selected == 'software': print('No usable requested GPU/runtime detected. The editor will use software processing.')
