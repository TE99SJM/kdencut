"""Start the native editor with tested session capabilities and safe defaults."""
import json
import os
from pathlib import Path
import sys
from hardware import detect, environment
from speech import prepare_models


def main():
    hardware = detect(os.environ.get('KDENCUT_GPU','auto'))
    folder = Path('/config/.local/share/kdencut')
    folder.mkdir(parents=True, exist_ok=True)
    temporary = folder/'hardware.json.tmp'
    temporary.write_text(json.dumps(hardware))
    temporary.replace(folder/'hardware.json')
    os.environ.update(environment(hardware))
    models = prepare_models(Path(os.environ.get('XDG_CACHE_HOME', '/config/.cache')))
    os.environ['KDENCUT_WHISPER_MODELS'] = str(models)
    print('KdenCut processing: '+hardware['backend'], flush=True)
    os.execvp('kdenlive', ['kdenlive', *sys.argv[1:]])


if __name__ == '__main__': main()
