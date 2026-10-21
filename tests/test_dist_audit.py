"""Source-only distribution checks; run without game data or console keys."""
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("audit", Path(__file__).resolve().parents[1] / "scripts/dist_audit.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class AuditTests(unittest.TestCase):
    def test_allowed_source(self):
        self.assertFalse(audit.check_name("src/ours.cpp", source_only=True))
        self.assertFalse(audit.check_data(b"// synthetic test"))

    def test_rejected_paths(self):
        for name in ("generated/main.cpp", "exefs/main", "prod.keys", "game.nsp",
                     "game.dll", "textures/kart.png", "../leak.md"):
            with self.subTest(name=name):
                self.assertTrue(audit.check_name(name, source_only=True))

    def test_key_and_game_magic(self):
        self.assertTrue(audit.check_data(b"master_key_00 = " + b"a" * 32))
        self.assertTrue(audit.check_data(b"NSO0" + b"\0" * 64))
        self.assertTrue(audit.check_data(b"\0" * 0x200 + b"NCA3"))

    def test_zip_members(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "source.zip"
            with zipfile.ZipFile(path, "w") as f:
                f.writestr("README.md", "authored source")
            self.assertFalse(audit.archive_audit(path))
            with zipfile.ZipFile(path, "a") as f:
                f.writestr("generated/code.cpp", "game code")
            self.assertTrue(audit.archive_audit(path))

    def test_tar_and_unknown(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "source.tar.gz"
            with tarfile.open(path, "w:gz") as f:
                item = tarfile.TarInfo("main.cpp")
                item.size = 3
                f.addfile(item, io.BytesIO(b"abc"))
            self.assertFalse(audit.archive_audit(path))
            unknown = Path(directory) / "unknown.7z"
            unknown.write_bytes(b"opaque")
            self.assertTrue(audit.candidate_audit(unknown))


if __name__ == "__main__":
    unittest.main()
