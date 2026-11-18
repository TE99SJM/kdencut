"""Expose the bundled Whisper model in Kdenlive's persistent model directory."""
from pathlib import Path


def prepare_models(cache, bundled=Path('/opt/kdencut-models')):
    target = Path(cache)/'whisper'
    target.mkdir(parents=True, exist_ok=True)
    for model in bundled.glob('*.pt'):
        destination = target/model.name
        if not destination.exists() and not destination.is_symlink():
            destination.symlink_to(model)
    return target
