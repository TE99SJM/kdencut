"""Preserve user models and keep speech usable without a first-run download."""
from pathlib import Path
import tempfile
import unittest
import test_media_server as common
from speech import prepare_models


class SpeechSetupTests(unittest.TestCase):
    def test_phone_chooser_accepts_lottie_formats(self):
        for extension in ['json', 'lottie', 'rawr', 'tgs', 'lot']:
            self.assertEqual(common.media.media_name('animation.'+extension), 'animation.'+extension)

    def test_bundled_model_link_and_existing_models_are_preserved(self):
        with tempfile.TemporaryDirectory() as root:
            base = Path(root); bundled = base/'bundled'; bundled.mkdir()
            (bundled/'tiny.pt').write_bytes(b'bundled')
            target = prepare_models(base/'cache', bundled)
            self.assertEqual((target/'tiny.pt').read_bytes(), b'bundled')
            (target/'tiny.pt').unlink(); (target/'tiny.pt').write_bytes(b'user model')
            (target/'large.pt').write_bytes(b'larger model')
            prepare_models(base/'cache', bundled)
            self.assertEqual((target/'tiny.pt').read_bytes(), b'user model')
            self.assertEqual((target/'large.pt').read_bytes(), b'larger model')
