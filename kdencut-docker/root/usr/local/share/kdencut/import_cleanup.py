"""Conservative, explicit import cleanup. Never remove media or NAS originals."""
from pathlib import Path
from urllib.parse import unquote
import html
import json
import os
import re
import shutil
import time
import xml.etree.ElementTree as ET

TOKEN = re.compile(r"^[0-9a-f]{32}$")


class ImportCleanup:
    def __init__(self, imports, project_roots, live_usage):
        self.imports = Path(imports).resolve()
        self.roots = [Path(root).resolve() for root in project_roots]
        self.live_usage = live_usage
        self.trash = self.imports / '.trash'

    def references(self):
        live = self.live_usage()
        if not live.get('available') or time.time()*1000 - live.get('at', 0) > 10000:
            raise ValueError('The editor could not check the open project. Try again when it finishes loading.')
        texts = [live.get('xml', '')]
        scanned = set()
        for root in self.roots:
            if not root.is_dir():
                raise ValueError('A project storage location is unavailable. Reconnect it before cleanup.')
            def unreadable(error):
                raise ValueError('A project folder could not be scanned. Check its permissions before cleanup.') from error
            for folder, dirs, files in os.walk(root, followlinks=False, onerror=unreadable):
                dirs[:] = [name for name in dirs if name not in ('.cache', '.git') and (Path(folder)/name).resolve() != self.imports]
                if any((Path(folder)/name).is_symlink() for name in dirs):
                    raise ValueError('A linked project folder cannot be checked. Use a directly mounted project folder before cleanup.')
                for name in files:
                    path = Path(folder)/name
                    # saveSceneList() stores JPEG previews beside XML backups.
                    # Keep the broad project/autosave scan, excluding only these
                    # known thumbnails in Kdenlive's backup directory.
                    if path.parent.name == '.backup' and name.lower().endswith('.kdenlive.jpg'):
                        continue
                    is_project = '.kdenlive' in name.lower() or path.suffix.lower() == '.mlt' or 'stalefiles' in path.parts
                    if not is_project or path in scanned: continue
                    scanned.add(path)
                    if path.is_symlink(): raise ValueError('A linked project cannot be checked. Keep it in a directly mounted project folder.')
                    try:
                        data = path.read_text(encoding='utf-8')
                        ET.fromstring(data)
                    except (OSError, UnicodeError, ET.ParseError) as error:
                        raise ValueError('A project or autosave could not be read. Cleanup is blocked to protect its media.') from error
                    texts.append(data)
        # UUID import folders are retained for relative paths, file URLs, speed
        # producers, proxies, title images and nested XML inside properties.
        combined = '\n'.join(html.unescape(unquote(text)).replace('\\/', '/') for text in texts)
        return combined, len(scanned)

    def candidates(self, protected=(), trash=False):
        if self.trash.is_symlink(): raise ValueError('Unsafe trash location')
        references, count = self.references()
        base = self.trash if trash else self.imports
        rows = []
        if not base.exists(): return {'files': rows, 'projects': count}
        for folder in base.iterdir():
            if not TOKEN.fullmatch(folder.name) or folder.is_symlink() or not folder.is_dir(): continue
            if folder.name in protected or folder.name in references: continue
            marker = folder/'.kdencut-import.json'
            if marker.is_symlink() or not marker.is_file(): continue  # Never infer ownership of legacy or user-created folders.
            try:
                metadata = json.loads(marker.read_text())
                name = metadata['name']
                if not isinstance(name, str) or Path(name).name != name: continue
                path = folder/name
                if path.is_symlink() or not path.is_file(): continue
                if set(p.name for p in folder.iterdir()) != {name, marker.name}: continue
                if not trash and time.time() - metadata['created'] < 24*3600: continue
                rows.append({'id':folder.name, 'name':name, 'size':path.stat().st_size})
            except (OSError, ValueError, KeyError, TypeError):
                continue
        return {'files': rows, 'projects': count}

    def apply(self, ids, action, protected=()):
        if self.trash.is_symlink(): raise ValueError('Unsafe trash location')
        if action not in ('trash', 'restore', 'purge') or not isinstance(ids, list) or not ids or len(ids) > 500:
            raise ValueError('Select between 1 and 500 imported files')
        if any(not isinstance(token, str) or not TOKEN.fullmatch(token) for token in ids): raise ValueError('Invalid import selection')
        if action == 'restore':
            for token in ids:
                source, destination = self.trash/token, self.imports/token
                if source.is_symlink() or not source.is_dir() or destination.exists(): raise ValueError('Cannot safely restore this import')
                source.rename(destination)
            return
        eligible = {row['id'] for row in self.candidates(protected, trash=action == 'purge')['files']}
        if not set(ids).issubset(eligible): raise ValueError('Project usage changed. Review unused imports again.')
        if self.trash.is_symlink(): raise ValueError('Unsafe trash location')
        self.trash.mkdir(mode=0o700, exist_ok=True)
        for token in ids:
            if action == 'trash':
                destination = self.trash/token
                if destination.exists(): raise ValueError('A recoverable copy already exists')
                (self.imports/token).rename(destination)
            else:
                shutil.rmtree(self.trash/token)

    def trashed(self):
        if self.trash.is_symlink(): raise ValueError('Unsafe trash location')
        result = []
        if self.trash.is_dir():
            for folder in self.trash.iterdir():
                if TOKEN.fullmatch(folder.name) and folder.is_dir() and not folder.is_symlink():
                    marker = folder/'.kdencut-import.json'
                    if marker.is_symlink(): continue
                    try:
                        result.append({'id':folder.name, 'name':json.loads(marker.read_text())['name']})
                    except (OSError, ValueError, KeyError): continue
        return result
