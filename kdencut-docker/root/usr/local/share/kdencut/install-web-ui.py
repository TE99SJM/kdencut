"""Install the mobile viewport adapter into the image's Selkies dashboards."""
from pathlib import Path
import hashlib
import re
import shutil


def install(root=Path('/usr/share/selkies'), assets=Path(__file__).parent):
    version = hashlib.sha256(b''.join((assets / f'{name}.{suffix}').read_bytes()
                                    for name in ('mobile', 'media') for suffix in ('js', 'css'))).hexdigest()[:12]
    pages = list(root.glob('*/index.html'))
    if not pages:
        raise RuntimeError('No Selkies dashboard index.html found; mobile UI was not installed')
    for page in pages:
        source = page.read_text()
        if '</head>' not in source:
            raise RuntimeError(f'Missing head in {page}')
        if 'kdencut-mobile.js' not in source:
            source = source.replace('</head>', '''<link rel="stylesheet" href="kdencut-mobile.css">
<script defer src="kdencut-mobile.js"></script>
</head>''', 1)
            # Existing Selkies viewport content is otherwise preserved.
            source = source.replace('name="viewport" content="', 'name="viewport" content="viewport-fit=cover, ', 1)
        if 'kdencut-media.js' not in source:
            source = source.replace('</head>', '<link rel="stylesheet" href="kdencut-media.css">\n<script defer src="kdencut-media.js"></script>\n</head>', 1)
        # Existing sessions can otherwise retain the old desktop resolution adapter.
        for name in ('mobile', 'media'):
            for suffix in ('css', 'js'):
                source = re.sub(r'kdencut-' + name + r'\.' + suffix + r'(?:\?[^"\s>]*)?',
                                f'kdencut-{name}.{suffix}?v={version}', source)
                shutil.copyfile(assets / f'{name}.{suffix}', page.parent / f'kdencut-{name}.{suffix}')
        page.write_text(source)
    return len(pages)


if __name__ == '__main__':
    print(f'Installed KdenCut mobile UI in {install()} dashboard(s)')
    config = Path('/defaults/default.conf')
    source = config.read_text()
    if 'kdencut-media/' not in source:
        location = '''  location SUBFOLDERkdencut-media/ {
    proxy_pass http://127.0.0.1:8766/;
    proxy_set_header Host $http_host;
    proxy_http_version 1.1;
    proxy_request_buffering off;
    proxy_buffering off;
    client_max_body_size 0;
    client_body_timeout 120s;
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
  }
'''
        marker = '  location SUBFOLDER {'
        if marker not in source:
            raise RuntimeError('Cannot install media endpoint in nginx defaults')
        config.write_text(source.replace(marker, location + marker))
