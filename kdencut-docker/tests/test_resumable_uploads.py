"""Chunk persistence, lost-response retries, boundaries and cancellation."""
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch
import test_media_server as common
media = common.media


class ResumableTests(unittest.TestCase):
    setUp = common.MediaTests.setUp
    def begin(self, size=9):
        return self.store.begin_upload(self.id, 'clip.mov', size, 'file-identity')

    def chunk(self, token, offset, payload, size=None):
        return self.store.receive_chunk(self.id, token, offset, len(payload) if size is None else size, io.BytesIO(payload))

    def test_resume_after_disconnect_and_service_restart(self):
        token = self.begin()['token']
        self.chunk(token, 0, b'first')
        with self.assertRaisesRegex(ValueError, 'interrupted'):
            self.chunk(token, 5, b'x', size=4)
        self.store = media.MediaStore(self.root/'bridge', self.root/'imports', {'media': self.folder})
        state = self.begin()
        self.assertEqual(state['token'], token)
        self.assertEqual(state['offset'], 5)
        self.assertTrue(self.chunk(token, 5, b'last')['complete'])
        self.store.finish(self.id, [token])
        result = json.loads((self.root/f'bridge/{self.id}.json').read_text())
        self.assertEqual(Path(result['paths'][0]).read_bytes(), b'firstlast')
        self.assertEqual(json.loads(self.store.uploads_file.read_text()), {})

    def test_retry_written_chunk_is_idempotent(self):
        token = self.begin()['token']
        self.chunk(token, 0, b'first')
        self.assertEqual(self.chunk(token, 0, b'first')['offset'], 5)
        with self.assertRaisesRegex(ValueError, 'differs'):
            self.chunk(token, 0, b'other')
        self.assertEqual(self.begin()['offset'], 5)

    def test_boundaries_and_identity(self):
        token = self.begin()['token']
        for offset, data, length in [(1, b'a', 1), (0, b'', media.CHUNK_SIZE + 1), (0, b'too much data', 13)]:
            with self.assertRaises(ValueError): self.chunk(token, offset, data, length)
        other = self.store.begin_upload(self.id, 'clip.mov', 9, 'different-file')['token']
        self.assertNotEqual(token, other)
        self.assertEqual(self.begin()['offset'], 0)

    def test_complete_upload_survives_service_restart(self):
        token = self.begin(5)['token']
        self.chunk(token, 0, b'video')
        self.store = media.MediaStore(self.root/'bridge', self.root/'imports', {'media': self.folder})
        self.assertTrue(self.begin(5)['complete'])
        self.store.finish(self.id, [token])
        self.assertTrue((self.root/f'bridge/{self.id}.json').exists())

    def test_crash_after_native_handoff_cannot_delete_imported_media(self):
        token = self.begin(5)['token']
        self.chunk(token, 0, b'video')
        path = Path(self.store.files[token]['path'])
        atomic_json = media.atomic_json
        def crash_after_publish(destination, data):
            atomic_json(destination, data)
            if Path(destination).name == self.id + '.json':
                # The native editor has consumed the ready message, then the
                # service dies before returning the HTTP response.
                Path(destination).unlink()
                (self.root/'bridge/request.json').unlink()
                raise RuntimeError('simulated process exit after handoff')
        with patch.object(media, 'atomic_json', side_effect=crash_after_publish):
            with self.assertRaisesRegex(RuntimeError, 'simulated process exit'):
                self.store.finish(self.id, [token])
        self.store = media.MediaStore(self.root/'bridge', self.root/'imports', {'media':self.folder})
        self.store.status()
        self.assertEqual(path.read_bytes(), b'video')

    def test_browsing_completed_pending_upload_keeps_selected_file(self):
        token = self.begin(5)['token']
        self.chunk(token, 0, b'video')
        path = Path(self.store.files[token]['path'])
        self.store.roots['imports'] = self.store.imports
        # A failed later file can leave completed files visible in Imported media.
        self.store.select(self.id, 'imports', [str(path.relative_to(self.store.imports))])
        (self.root/'bridge/request.json').unlink()
        self.store.status()
        self.assertEqual(path.read_bytes(), b'video')
        self.assertEqual(self.store.uploads, {})

    def test_cancel_and_native_close_remove_partial_chunks(self):
        token = self.begin()['token']
        self.chunk(token, 0, b'first')
        self.store.cancel(self.id)
        self.assertFalse(list(self.store.imports.iterdir()))
        with self.assertRaises(ValueError): self.chunk(token, 5, b'last')

    def test_native_close_removes_resume_metadata(self):
        token = self.begin()['token']
        self.chunk(token, 0, b'first')
        (self.root/'bridge/request.json').unlink()
        self.store.status()
        self.assertEqual(self.store.uploads, {})
        self.assertFalse(list(self.store.imports.iterdir()))
