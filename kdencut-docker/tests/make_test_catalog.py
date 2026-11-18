"""Build a tiny gettext fixture without depending on the host's translations."""
from pathlib import Path
import struct
import sys

messages = {
    '': 'Content-Type: text/plain; charset=UTF-8\nLanguage: fr\n',
    'Home': 'Accueil test',
    'Pause': 'Pause test',
    'Playhead %1': 'Position test %1',
}
keys = sorted(messages)
originals = [key.encode() for key in keys]
translations = [messages[key].encode() for key in keys]
count = len(keys)
offset = 28 + count * 16
tables, content = [], b''
for strings in (originals, translations):
    for value in strings:
        tables += [len(value), offset + len(content)]
        content += value + b'\0'
data = struct.pack('<7I', 0x950412de, 0, count, 28, 28 + count * 8, 0, 0)
data += struct.pack('<' + 'I' * len(tables), *tables) + content
target = Path(sys.argv[1]) / 'fr/LC_MESSAGES/kdencut-test.mo'
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(data)
