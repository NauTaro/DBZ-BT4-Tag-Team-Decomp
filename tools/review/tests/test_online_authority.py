"""Exercise authority decisions and release verification without touching a service."""
from pathlib import Path
import importlib.util
import json
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
spec = importlib.util.spec_from_file_location('online_authority', ROOT / 'port/tools/online_authority.py')
authority = importlib.util.module_from_spec(spec)
spec.loader.exec_module(authority)


class AuthorityTests(unittest.TestCase):
    def test_only_global_failure_changes_a_healthy_alternative(self):
        previous = {'active': 'primary'}
        for status in (400, 401, 403, 404, 422, 429):
            self.assertEqual(authority.choose(previous, {'primary': status, 'secondary': 200}), 'primary')
        for status in (0, 402, 500, 503):
            self.assertEqual(authority.choose(previous, {'primary': status, 'secondary': 200}), 'secondary')
        self.assertEqual(authority.choose(previous, {'primary': 503, 'secondary': 503}), 'primary')

    def test_healthy_reserve_does_not_oscillate_to_primary(self):
        statuses = {'primary': 200, 'secondary': 200}
        self.assertEqual(authority.choose({'active': 'secondary'}, statuses), 'secondary')
        self.assertEqual(authority.choose({'active': 'secondary'}, statuses, 'primary'), 'primary')

    def test_draft_and_prerelease_cannot_activate_gate(self):
        config = {'release_tag': 'v0.1.39.5-R4'}
        for flags in ({'draft': True}, {'prerelease': True}, {'published_at': None}):
            release = dict(draft=False, prerelease=False, published_at='2026-11-06T00:00:00Z',
                           tag_name=config['release_tag'], assets=[])
            release.update(flags)
            with self.assertRaises(authority.AuthorityError):
                authority.verify_release(config, bytes(32), lambda *a, **k: (200, json.dumps(release).encode()))

    def test_verifies_complete_archives_not_just_metadata(self):
        seed = bytes(range(32))
        public = authority.update_key.public_key(seed)
        data = b'archive test fixture' * 100
        signature = authority.update_key.sign(seed, data).hex().encode()
        config = {'release_tag': 'v0.1.39.5-R4'}
        assets = [dict(name='BT4decomp-' + p + '.zip' + suffix, size=len(data) if not suffix else len(signature))
                  for p in authority.PLATFORMS for suffix in ('', '.sig')]
        release = dict(draft=False, prerelease=False, published_at='2026-11-06T00:00:00Z',
                       tag_name=config['release_tag'], assets=assets)
        def fetch(url, **kwargs):
            return 200, json.dumps(release).encode() if '/releases/tags/' in url else signature if url.endswith('.sig') else data
        result = authority.verify_release(config, public, fetch)
        self.assertEqual(len(result), 3)
        def corrupt(url, **kwargs):
            status, body = fetch(url, **kwargs)
            return status, b'x' + body[1:] if url.endswith('.zip') else body
        with self.assertRaises(authority.AuthorityError):
            authority.verify_release(config, public, corrupt)

    def test_disabled_configuration_performs_no_network_or_writes(self):
        with tempfile.TemporaryDirectory() as temp:
            args = ['online_authority.py', '--config', str(ROOT / 'supabase/online-routing.json'),
                    '--state-dir', temp, '--prepare']
            with patch.object(sys, 'argv', args), patch.object(authority, 'request') as request, \
                 patch.object(authority, 'write_config') as write:
                authority.main()
                request.assert_not_called()
                write.assert_not_called()
                self.assertEqual(list(Path(temp).iterdir()), [])

    def test_fencing_refuses_unpublished_policy(self):
        config = dict(schema=1, enabled=True, minimum='0.1.39.5-R4', release_tag='v0.1.39.5-R4',
                      require_update=True, capability=1, policy_seconds=600, admission_seconds=900, automatic_return=False)
        now = 1800000000
        seed = bytes(range(32))
        public = authority.update_key.public_key(seed)
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            path = directory / 'config.json'
            path.write_text(json.dumps(config), encoding='utf-8')
            authority.save_signed(directory / 'policy.json', dict(schema=1, epoch=2, issued_at=now, expires_at=now+600,
                capability=1, active='secondary', minimum=config['minimum'], require_update=True), seed, authority.DOMAIN)
            authority.save_signed(directory / 'state.json', {'schema': 1, 'admission_until': {'secondary': now+900}},
                                 seed, b'BT4-ONLINE-LEASES-V1\n')
            args = ['online_authority.py', '--config', str(path), '--state-dir', temp, '--activate-published']
            with patch.object(sys, 'argv', args), patch.object(authority.time, 'time', return_value=now), \
                 patch.object(authority.update_key, 'header_key', return_value=public), \
                 patch.object(authority, 'request', return_value=(404, b'')), patch.object(authority, 'write_config') as write:
                with self.assertRaises(authority.AuthorityError):
                    authority.main()
                write.assert_not_called()


if __name__ == '__main__':
    unittest.main()
