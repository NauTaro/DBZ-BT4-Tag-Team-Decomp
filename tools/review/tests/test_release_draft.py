from pathlib import Path
import copy
import hashlib
import importlib.util
import tempfile
import unittest
from urllib.parse import parse_qs, urlsplit

ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('release_draft',ROOT/'port/tools/release_draft.py')
draft=importlib.util.module_from_spec(spec);spec.loader.exec_module(draft)


class ReleaseDraftTests(unittest.TestCase):
    def test_existing_temporary_tag_and_legacy_title_are_reused(self):
        old=dict(id=42,tag_name='untagged-temporary',name='0.1.39.5 R4',draft=True)
        self.assertIs(draft.select_draft([old],'v0.1.39.5-R4','BT4decomp 0.1.39.5 R4'),old)

    def test_published_or_duplicate_candidate_cannot_be_replaced(self):
        old=dict(id=42,tag_name='v0.1.39.5-R4',name='BT4decomp 0.1.39.5 R4',draft=False)
        with self.assertRaises(ValueError):draft.select_draft([old],old['tag_name'],old['name'])
        old['draft']=True
        with self.assertRaises(ValueError):draft.select_draft([old,dict(old,id=43)],old['tag_name'],old['name'])

    def test_upload_by_stable_id_is_repeatable_with_temporary_draft_tag(self):
        state=dict(id=42,tag_name='untagged-temporary',name='0.1.39.5 R4',draft=True,assets=[],
                   upload_url='https://uploads.github.com/repos/team/game/releases/42/assets{?name,label}',
                   html_url='https://github.com/team/game/releases/tag/untagged-temporary')
        calls=[]
        def api(method,url,data=None,binary=False):
            calls.append((method,url))
            path=urlsplit(url).path
            if method=='GET' and path.endswith('/releases'):return [copy.deepcopy(state)]
            if method=='GET':return copy.deepcopy(state)
            if method=='PATCH':
                self.assertNotIn('draft',data)
                state.update(data);state['tag_name']='untagged-temporary'
                return copy.deepcopy(state)
            if method=='POST':
                self.assertTrue(binary)
                asset=dict(id=len(state['assets'])+100,name=parse_qs(urlsplit(url).query)['name'][0],
                           size=len(data),digest='sha256:'+hashlib.sha256(data).hexdigest(),state='uploaded')
                state['assets'].append(asset);return copy.deepcopy(asset)
            self.fail('Unexpected operation: '+method)
        with tempfile.TemporaryDirectory() as folder:
            assets=[]
            for name in ('windows.zip','windows.zip.sig','linux.zip'):
                p=Path(folder)/name;p.write_bytes(name.encode());assets.append(p)
            for _ in range(2):
                result=draft.refresh('team/game','v0.1.39.5-R4','BT4decomp 0.1.39.5 R4','abc','notes',assets,api)
                self.assertEqual(result['id'],42);self.assertTrue(result['draft'])
            self.assertEqual(sum(method=='POST' for method,_ in calls),len(assets))
            self.assertFalse(any(method=='DELETE' for method,_ in calls))

    def test_unexpected_assets_stop_before_deletion_or_upload(self):
        state=dict(id=42,tag_name='v0.1.39.5-R4',name='BT4decomp 0.1.39.5 R4',draft=True,
                   assets=[dict(name='language-pack.bt4lang')])
        calls=[]
        def api(method,url,data=None,binary=False):
            calls.append(method)
            if '/releases?' in url:return [state]
            return state
        with self.assertRaisesRegex(ValueError,'Unexpected assets'):
            draft.refresh('team/game',state['tag_name'],state['name'],'abc','notes',[],api)
        self.assertNotIn('DELETE',calls);self.assertNotIn('POST',calls)


if __name__=='__main__':unittest.main()
