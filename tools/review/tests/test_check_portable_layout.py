"""Portable layout checks need neither game data nor a Windows toolchain."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


CHECKER = Path(__file__).resolve().parents[1] / 'check_portable_layout.py'
spec = importlib.util.spec_from_file_location('check_portable_layout', CHECKER)
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


class PortableLayoutTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'BT4 instalación'
        self.root.mkdir()

    def cli(self, root):
        return subprocess.run(
            [sys.executable, str(CHECKER), str(root)],
            cwd=self.temp.name, capture_output=True, text=True, check=False)

    def test_empty_root_is_a_valid_plan(self):
        self.assertTrue(layout.check(self.root)['passed'])

    def test_all_declared_entries_are_accepted(self):
        policy = json.loads(layout.POLICY_PATH.read_text(encoding='utf8'))
        for name in policy['root_directories']:
            (self.root / name).mkdir()
        for name in policy['root_files']:
            (self.root / name).touch()
        self.assertTrue(layout.check(self.root)['passed'])

    def test_names_are_case_insensitive(self):
        (self.root / 'SAVES').mkdir()
        (self.root / 'SDL3.DLL').touch()
        self.assertTrue(layout.check(self.root)['passed'])

    def test_unexpected_entries_are_sorted_and_preserved(self):
        (self.root / 'report.txt').write_bytes(b'keep this report')
        (self.root / 'backup').mkdir()
        result = layout.check(self.root)
        self.assertFalse(result['passed'])
        self.assertEqual(result['unexpected_root_entries'], ['backup', 'report.txt'])
        self.assertEqual(result['files_deleted'], [])
        self.assertEqual((self.root / 'report.txt').read_bytes(), b'keep this report')
        self.assertTrue((self.root / 'backup').is_dir())

    def test_file_cannot_replace_an_allowed_directory(self):
        (self.root / 'saves').touch()
        self.assertEqual(layout.check(self.root)['unexpected_root_entries'], ['saves'])

    def test_directory_cannot_replace_an_allowed_file(self):
        (self.root / 'SDL3.dll').mkdir()
        self.assertEqual(layout.check(self.root)['unexpected_root_entries'], ['SDL3.dll'])

    def test_user_data_is_untouched(self):
        contents = {
            'gamedata/custom.bin': b'game data',
            'saves/player.dat': b'player save',
            'bt3_settings.txt': b'fullscreen=1\n',
            'online_name.txt': b'player',
        }
        for name, content in contents.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.assertTrue(layout.check(self.root)['passed'])
        for name, content in contents.items():
            self.assertEqual((self.root / name).read_bytes(), content)

    def test_cli_uses_checkout_policy_from_another_working_directory(self):
        result = self.cli(self.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertTrue(report['passed'])
        self.assertEqual(report['root'], str(self.root.resolve()))

    def test_cli_returns_one_for_unexpected_entries(self):
        (self.root / 'extra.txt').touch()
        result = self.cli(self.root)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(json.loads(result.stdout)['unexpected_root_entries'], ['extra.txt'])

    def test_cli_returns_two_for_missing_directory(self):
        result = self.cli(self.root / 'missing')
        self.assertEqual(result.returncode, 2, result.stderr)
        report = json.loads(result.stdout)
        self.assertFalse(report['passed'])
        self.assertIn('error', report)
        self.assertEqual(report['files_deleted'], [])
        self.assertEqual(result.stderr, '')

    def test_cli_returns_two_for_file_instead_of_directory(self):
        path = self.root / 'file.txt'
        path.touch()
        result = self.cli(path)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn('error', json.loads(result.stdout))

    def test_cli_reports_missing_or_invalid_policy(self):
        checkout = Path(self.temp.name) / 'checkout'
        checker = checkout / 'tools/review/check_portable_layout.py'
        checker.parent.mkdir(parents=True)
        checker.write_text(CHECKER.read_text(encoding='utf8'), encoding='utf8')
        policy = checkout / 'port/release/PORTABLE_LAYOUT.json'
        policy.parent.mkdir(parents=True)
        for content in (None, '{invalid json'):
            with self.subTest(policy=content):
                if content is not None:
                    policy.write_text(content, encoding='utf8')
                result = subprocess.run(
                    [sys.executable, str(checker), str(self.root)],
                    capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, 2, result.stderr)
                report = json.loads(result.stdout)
                self.assertFalse(report['passed'])
                self.assertIn('error', report)
                self.assertEqual(report['files_deleted'], [])
                self.assertEqual(result.stderr, '')


if __name__ == '__main__':
    unittest.main()
