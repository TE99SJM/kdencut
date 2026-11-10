"""Install the mobile viewport adapter into the image's Selkies dashboards."""
from pathlib import Path
import shutil


def install(root=Path('/usr/share/selkies'), assets=Path(__file__).parent):
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
            page.write_text(source)
        for suffix in ('css', 'js'):
            shutil.copyfile(assets / f'mobile.{suffix}', page.parent / f'kdencut-mobile.{suffix}')
    return len(pages)


if __name__ == '__main__':
    print(f'Installed KdenCut mobile UI in {install()} dashboard(s)')
