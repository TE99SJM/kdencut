"""Browser regression checks. Run with Python + Playwright and Chromium installed."""
import importlib.util
import os
import shutil
from pathlib import Path
import tempfile
import unittest
from playwright.sync_api import sync_playwright

ASSETS = Path(__file__).resolve().parents[1] / 'root/usr/local/share/kdencut'


class MobileViewportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pw = sync_playwright().start()
        cls.browser = cls.pw.chromium.launch(executable_path=os.environ.get('CHROMIUM_PATH') or shutil.which('chromium'), args=['--no-sandbox'])

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.pw.stop()

    def page(self, width, height, touch=True):
        ctx = self.browser.new_context(viewport={'width': width, 'height': height}, has_touch=touch, is_mobile=touch, device_scale_factor=3 if touch else 1)
        self.addCleanup(ctx.close)
        page = ctx.new_page()
        # The stream is represented by its real Selkies container contract;
        # the network protocol/rendering itself requires a running container.
        page.route('http://kdencut.test/**', lambda route: route.fulfill(content_type='text/html', body='''
          <meta name="viewport" content="width=device-width, initial-scale=1">
          <style>body{margin:0}#app{width:100%;height:100vh;display:flex}.video-container{width:100%;height:100%;position:relative}</style>
          <div id="app"><div class="video-container"></div></div>
          <button class="virtual-keyboard-button" data-virtual-keyboard-button>Keyboard</button>
          <script>window.sent=[];addEventListener('message', e=>sent.push(e.data));</script>'''))
        page.goto('http://kdencut.test/')
        page.add_style_tag(path=str(ASSETS / 'mobile.css'))
        page.add_script_tag(path=str(ASSETS / 'mobile.js'))
        return page

    def settings(self, page, payload=None):
        page.evaluate("p => postMessage({type:'serverSettings',payload:p}, location.origin)", payload or {})
        page.wait_for_timeout(250)

    def check_fit(self, page):
        box = page.locator('.video-container').bounding_box()
        button = page.locator('.virtual-keyboard-button').bounding_box()
        size = page.evaluate("sent.filter(x=>x.type==='setManualResolution').at(-1)")
        self.assertGreaterEqual(size['width'], 320)
        self.assertGreaterEqual(size['height'], 280)
        self.assertLess(abs(size['width'] / size['height'] - box['width'] / box['height']), .01)
        self.assertLessEqual(box['y'] + box['height'], button['y'])
        self.assertLessEqual(button['y'] + button['height'], page.viewport_size['height'])
        self.assertEqual(page.evaluate("sent.filter(x=>x.type==='settings').at(-1).settings.scaling_dpi"), 96)

    def test_phone_sizes_and_rotation(self):
        for width, height in [(320,568), (375,667), (430,932), (932,430)]:
            with self.subTest(size=(width,height)):
                page = self.page(width, height)
                self.settings(page)
                self.check_fit(page)
                page.set_viewport_size({'width':height,'height':width})
                page.wait_for_timeout(300)
                self.check_fit(page)

    def test_keyboard_visual_viewport_and_reconnect(self):
        page = self.page(430,932)
        self.settings(page)
        page.set_viewport_size({'width':430,'height':400})
        page.wait_for_timeout(300)
        self.check_fit(page)
        count = page.evaluate("sent.filter(x=>x.type==='setManualResolution').length")
        self.settings(page)
        self.assertEqual(page.evaluate("sent.filter(x=>x.type==='setManualResolution').length"), count + 1)
        page.wait_for_timeout(500)
        self.assertEqual(page.evaluate("sent.filter(x=>x.type==='setManualResolution').length"), count + 1)

    def test_desktop_unchanged(self):
        page = self.page(1440,900,touch=False)
        self.settings(page)
        self.assertFalse(page.evaluate("document.documentElement.classList.contains('kdencut-mobile')"))
        self.assertEqual(page.evaluate("sent.filter(x=>x.type!=='serverSettings')"), [])

    def test_fixed_server_policy(self):
        page = self.page(430,932)
        self.settings(page, {'enable_resize':{'value':False}, 'scaling_dpi':{'locked':True,'value':144}})
        self.assertEqual(page.evaluate("sent.filter(x=>x.type==='setManualResolution'||x.type==='settings')"), [])
        self.assertTrue(page.evaluate("sent.some(x=>x.type==='setScaleLocally'&&x.value)"))

    def test_foreign_messages_ignored(self):
        page = self.page(430,932)
        page.evaluate("dispatchEvent(new MessageEvent('message',{origin:'https://elsewhere.test',source:window,data:{type:'serverSettings',payload:{}}}))")
        page.wait_for_timeout(250)
        self.assertFalse(page.evaluate("sent.some(x=>x.type==='setManualResolution')"))


class MobileInstallerTest(unittest.TestCase):
    def test_installer_idempotent(self):
        spec = importlib.util.spec_from_file_location('installer', ASSETS / 'install-web-ui.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dashboard = root / 'selkies-dashboard'
            dashboard.mkdir()
            page = dashboard / 'index.html'
            page.write_text('<head><meta name="viewport" content="width=device-width"></head><body></body>')
            self.assertEqual(module.install(root,ASSETS),1)
            first = page.read_text()
            self.assertEqual(module.install(root,ASSETS),1)
            self.assertEqual(page.read_text(), first)
            self.assertEqual(first.count('kdencut-mobile.js'),1)
            self.assertEqual(first.count('viewport-fit=cover'),1)
            self.assertIn('kdencut-mobile.js?v=',first)
            self.assertIn('kdencut-mobile.css?v=',first)
            changed = root / 'assets'
            changed.mkdir()
            for suffix in ('js', 'css'):
                shutil.copyfile(ASSETS / f'mobile.{suffix}', changed / f'mobile.{suffix}')
            with (changed / 'mobile.js').open('a') as script:
                script.write('\n// new version\n')
            module.install(root,changed)
            self.assertNotEqual(page.read_text(), first)
            self.assertEqual(page.read_text().count('kdencut-mobile.js'),1)
            module.install(root,ASSETS)
            self.assertEqual(page.read_text(), first)
            self.assertEqual((dashboard / 'kdencut-mobile.js').read_bytes(), (ASSETS / 'mobile.js').read_bytes())


if __name__ == '__main__':
    unittest.main()
