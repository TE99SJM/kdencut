"""Real browser file-picker/upload and storage navigation checks (CI)."""
import json
import os
from pathlib import Path
import tempfile
import threading
import time
import unittest
import uuid
from playwright.sync_api import sync_playwright
from test_media_server import media, ASSETS


class BrowserMediaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        (cls.root/'media').mkdir()
        cls.store = media.MediaStore(cls.root/'bridge', cls.root/'imports', {'media': cls.root/'media'})
        class Handler(media.Handler):
            def do_GET(self):
                if self.path == '/':
                    body = b'<html><head><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/media.css"></head><body><script defer src="/media.js"></script></body></html>'
                elif self.path in ('/media.js', '/media.css'):
                    body = (ASSETS/self.path[1:]).read_bytes()
                else:
                    self.path = self.path.removeprefix('/kdencut-media')
                    return super().do_GET()
                self.send_response(200)
                self.send_header('Content-Type', 'text/javascript' if self.path.endswith('.js') else 'text/css' if self.path.endswith('.css') else 'text/html')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers(); self.wfile.write(body)
            def do_POST(self):
                self.path = self.path.removeprefix('/kdencut-media')
                super().do_POST()
            def do_PUT(self):
                self.path = self.path.removeprefix('/kdencut-media')
                super().do_PUT()
        cls.server = media.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        cls.server.store = cls.store
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.url = f'http://127.0.0.1:{cls.server.server_port}'
        cls.pw = sync_playwright().start()
        cls.browser = cls.pw.chromium.launch(headless=True)

    @classmethod
    def tearDownClass(cls):
        cls.browser.close(); cls.pw.stop(); cls.server.shutdown(); cls.server.server_close(); cls.temp.cleanup()

    def setUp(self):
        self.context = self.browser.new_context(viewport={'width': 390, 'height': 760}, is_mobile=True, has_touch=True)
        self.addCleanup(self.context.close)
        self.page = self.context.new_page()
        self.errors = []
        self.page.on('pageerror', lambda error: self.errors.append(str(error)))
        self.id = str(uuid.uuid4())
        media.atomic_json(self.root/'bridge/request.json', {'id': self.id, 'created': time.time()*1000, 'kind': 'media'})
        self.page.goto(self.url)
        self.page.locator('#kdencut-media-dialog').wait_for(state='visible')

    def wait_result(self):
        result = self.root/f'bridge/{self.id}.json'
        for _ in range(100):
            if result.exists(): return json.loads(result.read_text())
            self.page.wait_for_timeout(100)
        self.fail('Native handoff not produced')

    def test_real_picker_multiple_files_and_bytes(self):
        self.page.get_by_role('button', name='This device').click()
        with self.page.expect_file_chooser() as chooser:
            self.page.get_by_label('Choose photos and videos').click()
        chooser.value.set_files([{'name': 'one.mov', 'mimeType': 'video/quicktime', 'buffer': b'first video'},
                                 {'name': 'two.jpg', 'mimeType': 'image/jpeg', 'buffer': b'second image'}])
        result = self.wait_result()
        self.assertEqual([Path(p).read_bytes() for p in result['paths']], [b'first video', b'second image'])
        self.page.locator('#kdencut-media-dialog').wait_for(state='hidden')
        self.assertEqual(self.errors, [])

    def test_folder_browse_and_native_handoff(self):
        (self.root/'media'/'Trips').mkdir(exist_ok=True)
        clip = self.root/'media/Trips/clip.mov'; clip.write_bytes(b'NAS mounted file')
        self.page.get_by_role('button', name='Server / NAS').click()
        self.page.get_by_role('button', name='▸ Trips').click()
        self.page.get_by_role('checkbox', name='clip.mov').check()
        self.page.get_by_role('button', name='Add selected (1)').click()
        self.assertEqual(self.wait_result()['paths'], [str(clip)])
        self.assertEqual(self.errors, [])

    def test_portrait_storage_long_names_and_selection(self):
        folder = self.root/'media'/'Portrait'
        folder.mkdir(exist_ok=True)
        filename = 'Long phone video filename ' + 'unbroken' * 18 + '.mov'
        for index in range(20): (folder/f'{index:02d}.mov').write_bytes(b'video')
        (folder/filename).write_bytes(b'video')
        self.page.reload()
        self.page.locator('#kdencut-media-dialog').wait_for(state='visible')
        # Server storage is immediately available; no desktop file dialog needed.
        self.assertEqual(self.page.get_by_role('button', name='Server / NAS').get_attribute('aria-pressed'), 'true')
        self.page.get_by_role('button', name='▸ Portrait').click()
        self.page.get_by_role('checkbox', name=filename, exact=True).check()
        for width, height in [(320,568),(390,760),(390,320)]:
            self.page.set_viewport_size({'width': width, 'height': height})
            self.assertTrue(self.page.locator('#kdencut-media-dialog').evaluate('(e)=>e.scrollWidth<=e.clientWidth'))
            content = self.page.locator('#kdencut-media-dialog > section')
            self.assertTrue(content.evaluate('(e)=>e.scrollWidth<=e.clientWidth'))
            content.evaluate('(e)=>e.scrollTop=0')
            add = self.page.get_by_role('button', name='Add selected (1)')
            box = add.bounding_box()
            viewport = content.bounding_box()
            self.assertGreaterEqual(box['y'], viewport['y'])
            self.assertLessEqual(box['y'] + box['height'], viewport['y'] + viewport['height'] + 1)
        self.page.set_viewport_size({'width':390, 'height':760})
        screenshot = os.environ.get('KDENCUT_MEDIA_SCREENSHOT')
        if screenshot: self.page.screenshot(path=screenshot.replace('.png', '-storage.png'))
        self.page.get_by_role('button', name='↑ Parent').click()
        self.page.get_by_role('button', name='Add selected', exact=True).wait_for()
        self.assertTrue(self.page.get_by_role('button', name='Add selected', exact=True).is_disabled())
        self.assertEqual(self.errors, [])

    def test_nas_form_small_screens_and_cancellation(self):
        self.page.get_by_role('button', name='Server / NAS').click()
        self.page.get_by_role('button', name='+ NAS').click()
        for width, height in [(320,568),(390,760),(844,390),(390,320)]:
            self.page.set_viewport_size({'width': width, 'height': height})
            box = self.page.locator('#kdencut-media-dialog').bounding_box()
            self.assertGreaterEqual(box['x'], 0); self.assertGreaterEqual(box['y'], 0)
            self.assertLessEqual(box['x']+box['width'], width)
            self.assertLessEqual(box['y']+box['height'], height)
            self.assertTrue(self.page.locator('#kdencut-media-dialog').evaluate('(e)=>e.scrollWidth<=e.clientWidth'))
            self.page.get_by_role('button', name='Connect and save').scroll_into_view_if_needed()
        self.page.set_viewport_size({'width': 390,'height':760})
        self.page.get_by_label('NAS address').fill('nas.local')
        self.page.get_by_label('Share name').fill('Video')
        screenshot = os.environ.get('KDENCUT_MEDIA_SCREENSHOT')
        if screenshot: self.page.screenshot(path=screenshot)
        self.page.get_by_role('button', name='Cancel', exact=True).click()
        self.assertEqual(self.wait_result()['status'], 'cancelled')
        self.assertEqual(self.errors, [])

    def test_cross_origin_and_missing_header_rejected(self):
        response = self.context.request.get(self.url+'/kdencut-media/sources')
        self.assertEqual(response.status, 403)
        response = self.context.request.post(self.url+'/kdencut-media/cancel', headers={'X-KdenCut-Media': '1','Origin':'https://elsewhere.test'}, data={'request': self.id})
        self.assertEqual(response.status, 403)


if __name__ == '__main__': unittest.main()
