"""Check runnable documentation and the protected deployment configuration."""
from pathlib import Path
import hashlib
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DocumentationTests(unittest.TestCase):
    def test_local_links_and_shell_examples(self):
        for path in (ROOT/'KDENCUT.md', ROOT/'kdencut-docker/README.md'):
            text = path.read_text()
            for target in re.findall(r'\]\(([^)]+)\)', text):
                if '://' in target or target.startswith('#'): continue
                self.assertTrue((path.parent/target.split('#')[0]).exists(), target)
            for block in re.findall(r'```bash\n(.*?)```', text, re.S):
                result = subprocess.run(['bash', '-n'], input=block, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_existing_ports_volumes_and_env_template_are_unchanged(self):
        # Compatibility contract captured from kdencut-touch-capcut at b50ccfa.
        expected = {
            'compose.yaml': '63c2bd0da8ee709ea20ac9760a00cf0e5cb2d9b2f8c42d30f835129eda37bb51',
            '.env.example': '78c117d4bf92b55685f602bc52d135d839ff6513a4e1c7cef3b3c0f6171393c4',
        }
        for filename, digest in expected.items():
            self.assertEqual(hashlib.sha256((ROOT/'kdencut-docker'/filename).read_bytes()).hexdigest(), digest, filename)


if __name__ == '__main__': unittest.main()
