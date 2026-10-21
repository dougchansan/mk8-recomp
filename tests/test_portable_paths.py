import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


audit = load("check-portable-paths")
package = load("package-native-checkpoint")


class PortablePaths(unittest.TestCase):
    def test_rejects_local_drives_inside_patches_and_json(self):
        for separator in ["/", "\\", "\\\\"]:
            text = '+set(ROOT "' + chr(71) + ':' + separator + 'work' + separator + 'source")'
            self.assertEqual(audit.violations(text), [(1, text)])

    def test_rejects_private_home_and_mount_paths(self):
        for path in ["/" + "Users/person/repo", "/" + "home/person/repo", "/" + "mnt/g/repo"]:
            self.assertTrue(audit.violations(path))

    def test_accepts_portable_configuration_and_urls(self):
        text = '''https://example.com/source
${CMAKE_CURRENT_SOURCE_DIR}/../..
$env:MK8R_ROOT
Env:SUYU_RECOMP_MAIN_HOLE
./local/build
python tests/native_platform/run.py --build ./build/platform'''
        self.assertEqual(audit.violations(text), [])

    def test_public_commands_redact_the_interpreter_and_source(self):
        python = chr(67) + ':\\Users\\person\\Python\\python.exe'
        source = chr(71) + ':\\checkout with spaces'
        self.assertEqual(package.public_command(
            [python, source + '\\tests\\run.py', '--build', source + '\\build'],
            {source: '<source>'}, python),
            ['python', '<source>\\tests\\run.py', '--build', '<source>\\build'])


if __name__ == '__main__':
    unittest.main()
