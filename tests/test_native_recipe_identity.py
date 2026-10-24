import hashlib
import json
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION = ROOT / 'integrations/belmante-runtime'


class NativeRecipeIdentity(unittest.TestCase):
    def test_license_and_patch_match_published_byte_identities(self):
        metadata = json.loads((INTEGRATION / 'UPSTREAM.json').read_text())
        for name, key in [('LICENSE.upstream', 'license_sha256'),
                          ('mk8d.patch', 'patch_sha256')]:
            digest = hashlib.sha256((INTEGRATION / name).read_bytes()).hexdigest()
            self.assertEqual(digest, metadata[key])
        self.assertNotIn(b'\r', (INTEGRATION / 'LICENSE.upstream').read_bytes())

    def test_license_checkout_uses_canonical_lf(self):
        name = 'integrations/belmante-runtime/LICENSE.upstream'
        result = subprocess.check_output(
            ['git', 'check-attr', 'eol', '--', name], cwd=ROOT, text=True).strip()
        self.assertEqual(result, name + ': eol: lf')


if __name__ == '__main__':
    unittest.main()
