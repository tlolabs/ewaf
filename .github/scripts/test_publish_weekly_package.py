import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import json
spec = importlib.util.spec_from_file_location('publish', Path(__file__).with_name('publish_weekly_package.py'))
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)

class PackageTests(unittest.TestCase):
    def test_platform_names(self):
        self.assertEqual(p.package_name('v1.0.7', 'windows-arm64'), 'EWAF-1.0.7-windows-ARM64.zip')
        self.assertEqual(p.package_name('v1.0.7-rc.2', 'linux-x64'), 'EWAF-1.0.7-linux-amd64.deb')
    def test_bad_tag(self):
        for tag in ('main', '../v1.0.7', 'v1.0.7\n', 'v1.0.7;evil'):
            with self.assertRaises(ValueError): p.package_name(tag, 'linux-x64')
    def test_inventory_and_digest(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); name = 'EWAF-1.0.7-linux-amd64.deb'
            file = root/name; file.write_bytes(b'package')
            side = root/(name+'.sha256'); side.write_text(p.digest(file)+'  '+name+'\n')
            self.assertEqual(p.validate_download(root,name), [side,file])
            file.write_bytes(b'changed')
            with self.assertRaises(ValueError): p.validate_download(root,name)
            side.write_text(p.digest(file)+'  '+name+'\n')
            (root/'extra').write_text('unexpected')
            with self.assertRaises(ValueError): p.validate_download(root,name)

class PublisherTests(unittest.TestCase):
    def env(self):
        return dict(GITHUB_REPOSITORY='owner/repo',RELEASE_TAG='v1.0.7',
            RELEASE_SHA='a'*40,RELEASE_TARGET='linux-x64',GITHUB_RUN_ID='99',
            GITHUB_RUN_ATTEMPT='1',GITHUB_EVENT_NAME='schedule')
    def test_missing_artifact_never_writes(self):
        with patch.dict(p.os.environ,self.env(),clear=True), patch.object(p,'gh',return_value='[{"artifacts":[]}]') as api:
            with self.assertRaisesRegex(ValueError, 'No verified package'):
                p.main()
            self.assertEqual(api.call_count,1)
    def test_retries_and_non_schedule_rejected(self):
        for key,value in [('GITHUB_RUN_ATTEMPT','2'),('GITHUB_EVENT_NAME','push')]:
            env=self.env();env[key]=value
            with patch.dict(p.os.environ,env,clear=True), patch.object(p,'gh') as api:
                with self.assertRaises(ValueError):p.main()
                api.assert_not_called()
    def test_moved_tag_rejected(self):
        art=json.dumps([{'artifacts':[{'name':'verified-v1.0.7-linux-x64','expired':False}]}])
        with patch.dict(p.os.environ,self.env(),clear=True), patch.object(p,'gh',side_effect=[art,json.dumps({'sha':'b'*40})]) as api:
            with self.assertRaises(ValueError):p.main()
            self.assertEqual(api.call_count,2)
    def test_draft_rejected(self):
        art=json.dumps([{'artifacts':[{'name':'verified-v1.0.7-linux-x64','expired':False}]}])
        with patch.dict(p.os.environ,self.env(),clear=True), patch.object(p,'gh',side_effect=[art,json.dumps({'sha':'a'*40}),json.dumps({'draft':True,'tag_name':'v1.0.7'})]) as api:
            with self.assertRaises(ValueError):p.main()
            self.assertEqual(api.call_count,3)

    def test_successful_upload_checks_digests_and_preserves_release_flags(self):
        self.run_publication()
    def test_identical_existing_package_is_not_uploaded(self):
        self.run_publication(existing=True)
    def test_different_existing_package_rejected_before_any_upload(self):
        self.run_publication(existing=True, conflict=True)
    def run_publication(self, existing=False, conflict=False):
        calls=[]; assets=[]
        name='EWAF-1.0.7-linux-amd64.deb'
        if existing:
            import hashlib
            assets.append({'name':name,'digest':'sha256:'+hashlib.sha256(b'package').hexdigest()})
        def fake(*args):
            calls.append(args)
            if args[:3] == ('api','--paginate','--slurp'):
                return json.dumps([{'artifacts':[{'name':'verified-v1.0.7-linux-x64','expired':False}]}])
            if args[:2] == ('api','repos/owner/repo/commits/v1.0.7'):
                return json.dumps({'sha':'a'*40})
            if args[:2] == ('api','repos/owner/repo/releases/tags/v1.0.7'):
                return json.dumps({'id':1,'draft':False,'tag_name':'v1.0.7','body':'Original notes','assets':assets})
            if args[:2] == ('run','download'):
                directory=Path(args[-1]);file=directory/'EWAF-1.0.7-linux-amd64.deb';file.write_bytes(b'package')
                (directory/(file.name+'.sha256')).write_text(p.digest(file)+'  '+file.name+'\n')
                return ''
            if args[:2] == ('release','download'):
                directory=Path(args[-1])
                (directory/name).write_bytes(b'wrong' if conflict else b'package')
                return ''
            if args[:2] == ('release','upload'):
                file=Path(args[3]);assets.append({'name':file.name,'digest':'sha256:'+p.digest(file)})
                return ''
            if args[:3] == ('api','--method','PATCH'):
                self.assertEqual(args[4],'-F')
                self.assertTrue(args[5].startswith('body=@'))
                body=Path(args[5][6:]).read_text()
                self.assertTrue(body.startswith('Original notes'))
                self.assertIn('Weekly platform qualification',body)
                return ''
            raise AssertionError(args)
        with patch.dict(p.os.environ,self.env(),clear=True),patch.object(p,'gh',side_effect=fake):
            if conflict:
                with self.assertRaisesRegex(ValueError,'Existing release asset differs'):
                    p.main()
                self.assertFalse(any(c[:2] == ('release','upload') for c in calls))
                return
            p.main()
        self.assertEqual(len(assets),3)
        uploads=[c for c in calls if c[:2] == ('release','upload')]
        self.assertTrue(uploads[0][3].endswith('.sha256'))
        if existing:
            self.assertFalse(any(Path(c[3]).name == name for c in uploads))
        self.assertFalse(any('create' in c or '--clobber' in c or '--latest' in c for c in calls))

if __name__ == '__main__': unittest.main()
