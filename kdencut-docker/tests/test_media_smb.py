"""Read actual SMB2 share files through smbprotocol; requires impacket (CI)."""
import json
from pathlib import Path
import tempfile
import threading
import time
import unittest
import uuid
from impacket.smbserver import SimpleSMBServer
from impacket.ntlm import compute_lmhash, compute_nthash
from test_media_server import media


class SmbIntegrationTests(unittest.TestCase):
    def test_smb_connection_browse_download_and_original(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            share = root/'share'; share.mkdir()
            clip = share/'clip.mov'; clip.write_bytes(b'NAS original video'*10000)
            server = SimpleSMBServer(listenAddress='127.0.0.1', listenPort=1445)
            server.setSMB2Support(True)
            server.addShare('Video', str(share), readOnly='yes')
            server.addCredential('mediauser', 0, compute_lmhash('test-secret'), compute_nthash('test-secret'))
            threading.Thread(target=server.start, daemon=True).start()
            store = media.MediaStore(root/'bridge', root/'imports', {})
            try:
                connection = store.connect({'server':'127.0.0.1','port':1445,'share':'Video','username':'mediauser','password':'test-secret'})
                rows = store.listing(connection['id'], '')
                self.assertEqual(rows[0]['name'], 'clip.mov')
                request = str(uuid.uuid4())
                media.atomic_json(root/'bridge/request.json', {'id':request, 'created':time.time()*1000})
                store.select(request, connection['id'], ['clip.mov'])
                for _ in range(300):
                    result_path = root/f'bridge/{request}.json'
                    if result_path.exists(): break
                    if store.jobs.get(request,{}).get('state') == 'error': self.fail(store.jobs[request]['error'])
                    time.sleep(.1)
                result = json.loads(result_path.read_text())
                self.assertEqual(Path(result['paths'][0]).read_bytes(), clip.read_bytes())
                self.assertEqual(clip.read_bytes(), b'NAS original video'*10000)
            finally:
                server.stop()


if __name__ == '__main__': unittest.main()
