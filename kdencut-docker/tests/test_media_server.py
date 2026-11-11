"""Exercise completed-file handoff, cancellation, safe paths, and NAS failures."""
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import time
import unittest
import uuid
from unittest.mock import patch

ASSETS = Path(__file__).resolve().parents[1] / 'root/usr/local/share/kdencut'
spec = importlib.util.spec_from_file_location('media_server', ASSETS / 'media_server.py')
media = importlib.util.module_from_spec(spec)
spec.loader.exec_module(media)


class MediaTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.folder = self.root / 'media'
        self.folder.mkdir()
        self.store = media.MediaStore(self.root/'bridge', self.root/'imports', {'media': self.folder})
        self.id = str(uuid.uuid4())
        media.atomic_json(self.root/'bridge/request.json', {'id': self.id, 'created': time.time()*1000, 'kind': 'media'})

    def test_upload_bytes_unique_names_and_completed_handoff(self):
        payload = b'video bytes' * 1000
        tokens = [self.store.receive(self.id, 'same.mov', len(payload), io.BytesIO(payload)) for _ in range(2)]
        self.assertNotEqual(self.store.files[tokens[0]]['path'], self.store.files[tokens[1]]['path'])
        self.assertFalse((self.root/f'bridge/{self.id}.json').exists())
        self.store.finish(self.id, tokens)
        result = json.loads((self.root/f'bridge/{self.id}.json').read_text())
        self.assertEqual(result['status'], 'ready')
        self.assertEqual([Path(path).read_bytes() for path in result['paths']], [payload, payload])
        self.assertFalse(list((self.root/'imports').rglob('*.part')))

    def test_interrupted_upload_is_removed(self):
        with self.assertRaisesRegex(ValueError, 'interrupted'):
            self.store.receive(self.id, 'video.mov', 20, io.BytesIO(b'short'))
        self.assertEqual(list((self.root/'imports').iterdir()), [])
        self.assertEqual(self.store.files, {})

    def test_cancel_during_transfer_does_not_publish(self):
        store, request_id = self.store, self.id
        class Cancelled(io.BytesIO):
            def read(self, count):
                store.cancel(request_id)
                return super().read(count)
        with self.assertRaises(ValueError):
            store.receive(request_id, 'video.mov', 5, Cancelled(b'bytes'))
        self.assertEqual(list((self.root/'imports').iterdir()), [])

    def test_native_cancel_cleans_uncommitted_uploads(self):
        self.store.receive(self.id, 'video.mov', 5, io.BytesIO(b'bytes'))
        (self.root/'bridge/request.json').unlink()
        self.store.status()
        self.assertEqual(list((self.root/'imports').iterdir()), [])

    def test_new_request_cannot_claim_old_upload(self):
        token = self.store.receive(self.id, 'video.mov', 5, io.BytesIO(b'bytes'))
        other = str(uuid.uuid4())
        media.atomic_json(self.root/'bridge/request.json', {'id': other, 'created': time.time()*1000})
        with self.assertRaisesRegex(ValueError, 'another import'):
            self.store.finish(other, [token])

    def test_paths_and_symlink_escape_rejected(self):
        outside = self.root/'private.mov'
        outside.write_bytes(b'private')
        (self.folder/'link.mov').symlink_to(outside)
        for path in ['../private.mov', '/private.mov', '..\\private.mov', 'link.mov', 'folder/../../private.mov']:
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.store.select(self.id, 'media', [path])
        for name in ['../x.mov', 'x\\y.mp4', 'script.py', '.hidden.mp4']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.store.receive(self.id, name, 5, io.BytesIO(b'bytes'))
        self.assertEqual(self.store.listing('media', ''), [])

    def test_mounted_media_keeps_original_path_and_bytes(self):
        original = self.folder/'clip.mov'
        original.write_bytes(b'original')
        self.store.select(self.id, 'media', ['clip.mov'])
        result = json.loads((self.root/f'bridge/{self.id}.json').read_text())
        self.assertEqual(result['paths'], [str(original)])
        self.assertEqual(original.read_bytes(), b'original')

    def test_credentials_private_and_never_returned(self):
        fields = {'server': 'nas.local', 'share': 'Video', 'username': 'owner', 'password': 'secret'}
        with patch.object(self.store, 'smb_list', return_value=[]):
            connection = self.store.connect(fields)
        self.assertNotIn('secret', json.dumps(self.store.sources()))
        self.assertEqual(self.store.connections_file.stat().st_mode & 0o777, 0o600)
        restored = media.MediaStore(self.root/'bridge', self.root/'imports', {'media': self.folder})
        self.assertEqual(restored.connections[connection['id']]['password'], 'secret')
        self.store.forget(connection['id'])
        self.assertEqual(self.store.sources()['connections'], [])

    def test_failed_connection_not_saved(self):
        with patch.object(self.store, 'smb_list', side_effect=PermissionError()):
            with self.assertRaises(PermissionError):
                self.store.connect({'server': 'nas', 'share': 'Video'})
        self.assertEqual(self.store.connections, {})

    def test_insufficient_space_does_not_write(self):
        with patch.object(media.shutil, 'disk_usage', return_value=type('Space', (), {'free': 0})()):
            with self.assertRaisesRegex(ValueError, 'Not enough'):
                self.store.receive(self.id, 'clip.mov', 5, io.BytesIO(b'bytes'))
        self.assertEqual(list((self.root/'imports').iterdir()), [])


if __name__ == '__main__':
    unittest.main()
