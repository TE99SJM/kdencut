"""Never delete referenced, recent, unowned or incompletely scanned imports."""
import json
import time
import unittest
from unittest.mock import patch
import test_media_server as common
from import_cleanup import ImportCleanup


class CleanupTests(unittest.TestCase):
    setUp = common.MediaTests.setUp

    def setup_cleanup(self):
        self.live = {'available':True, 'at':time.time()*1000, 'xml':'<clips/>'}
        self.clean = ImportCleanup(self.store.imports, [self.folder], lambda:self.live)

    def imported(self, name='video.mov', old=True):
        token, path = self.store.allocate(self.id, name)
        path.write_bytes(b'original bytes')
        common.media.atomic_json(path.parent/'.kdencut-import.json', {'name':name, 'created':time.time() - (90000 if old else 0)})
        return token, path

    def test_unused_quarantine_restore_and_permanent_delete(self):
        self.setup_cleanup(); token, path = self.imported()
        self.assertEqual([f['id'] for f in self.clean.candidates()['files']], [token])
        self.clean.apply([token], 'trash')
        self.assertFalse(path.exists())
        self.assertEqual(self.clean.trashed()[0]['name'], 'video.mov')
        self.clean.apply([token], 'restore')
        self.assertEqual(path.read_bytes(), b'original bytes')
        self.clean.apply([token], 'trash'); self.clean.apply([token], 'purge')
        self.assertEqual(self.clean.trashed(), [])

    def test_project_urls_relative_proxies_titles_and_live_bin_protect(self):
        self.setup_cleanup()
        for reference in ['../imports/{id}/video.mov', 'file:///config/Imports/{id}/video.mov', '0.5:/config/Imports/{id}/video.mov']:
            token, _ = self.imported()
            (self.folder/f'{token}.kdenlive').write_text('<mlt><property name="resource">'+reference.format(id=token)+'</property></mlt>')
        token, _ = self.imported()
        self.live['xml'] = f'<clips><property name="xmldata">&lt;image url="/config/Imports/{token}/video.mov"/&gt;</property></clips>'
        self.assertEqual(self.clean.candidates()['files'], [])

    def test_new_reference_blocks_apply_and_purge(self):
        self.setup_cleanup(); token, path = self.imported()
        self.clean.candidates()
        self.live['xml'] = f'<clip resource="{path}"/>'
        with self.assertRaisesRegex(ValueError, 'usage changed'): self.clean.apply([token], 'trash')
        self.live['xml'] = '<clips/>'; self.clean.apply([token], 'trash')
        (self.folder/'saved.kdenlive').write_text(f'<clip resource="{path}"/>')
        with self.assertRaisesRegex(ValueError, 'usage changed'): self.clean.apply([token], 'purge')
        self.clean.apply([token], 'restore')
        self.assertTrue(path.exists())

    def test_recent_pending_unowned_and_symlink_imports_are_kept(self):
        self.setup_cleanup(); self.imported(old=False)
        token, _ = self.imported()
        _, path = self.imported(); (path.parent/'.kdencut-import.json').unlink()
        _, link = self.imported(); link.unlink(); link.symlink_to(path)
        self.assertEqual(self.clean.candidates({token})['files'], [])

    def test_unreadable_project_and_stale_editor_fail_closed(self):
        self.setup_cleanup(); self.imported()
        bad = self.folder/'broken.kdenlive'; bad.write_text('not xml')
        with self.assertRaisesRegex(ValueError, 'blocked'): self.clean.candidates()
        bad.unlink(); self.live['at'] = 0
        with self.assertRaisesRegex(ValueError, 'open project'): self.clean.candidates()

    def test_linked_project_folder_and_trash_are_blocked(self):
        self.setup_cleanup(); self.imported()
        link = self.folder/'linked'; link.symlink_to(self.root/'imports', target_is_directory=True)
        # An import symlink is ignored as the import root; other project links block.
        link.unlink(); link.symlink_to(self.root/'bridge', target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'linked project'): self.clean.candidates()
        link.unlink(); self.clean.trash.symlink_to(self.folder, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Unsafe trash'): self.clean.trashed()

    def test_low_space_warning(self):
        with patch.object(common.media.shutil, 'disk_usage', return_value=type('Space', (), {'free':1024, 'total':1024**4})()):
            self.assertTrue(self.store.status()['space']['low'])
        with patch.object(common.media.shutil, 'disk_usage', return_value=type('Space', (), {'free':100*1024**3, 'total':1024**4})()):
            self.assertFalse(self.store.status()['space']['low'])
