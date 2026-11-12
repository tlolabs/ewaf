#!/usr/bin/env python3
"""Offline packaging checks: no Apple request, signing or application launch."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NotarizationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.log = self.root / 'calls'
        self.app = self.root / 'Example.app'
        (self.app / 'Contents').mkdir(parents=True)
        fake = '''#!/usr/bin/env python3
import json, os, pathlib, sys
name=pathlib.Path(sys.argv[0]).name
args=sys.argv[1:]
with open(os.environ['CALL_LOG'],'a') as f: f.write(name+' '+' '.join(args)+'\\n')
if name=='codesign' and '-dv' in args:
 print('Authority=Developer ID Application: Test (TEST123456)\\nCodeDirectory v=20500 flags=0x10000(runtime)',file=sys.stderr)
if name=='ditto':
 if '-x' in args: (pathlib.Path(args[-1])/'Example.app/Contents').mkdir(parents=True)
 else: pathlib.Path(args[-1]).touch()
if name=='security': print('1) ABCDEF "Developer ID Application: Test (TEST123456)"')
if name=='codesign' and '--verify' in args and os.environ.get('FAIL_SIGNATURE'): sys.exit(8)
if name=='xcrun' and args[:2]==['notarytool','history'] and os.environ.get('FAIL_HISTORY'): sys.exit(17)
if name=='xcrun' and args[:2]==['notarytool','submit']:
 value=os.environ.get('APPLE_STATUS','Accepted')
 print('invalid json' if value=='malformed' else json.dumps({'status':value}))
if name=='xcrun' and args[:2]==['stapler','validate'] and os.environ.get('FAIL_STAPLE'): sys.exit(7)
'''
        for name in ['codesign', 'ditto', 'xcrun', 'spctl', 'security']:
            p=self.bin/name;p.write_text(fake);p.chmod(0o755)
        self.env=dict(os.environ, PATH=str(self.bin)+os.pathsep+os.environ['PATH'], CALL_LOG=str(self.log), NOTARY_PROFILE='fixture-profile')
        self.env.pop('SIGNING_IDENTITY',None)
        self.env.pop('EWAF_PRODUCTION',None)
    def helper(self, **updates):
        return subprocess.run(['bash',str(ROOT/'script/notarize_app.sh'),str(self.app),str(self.root/'out')],env=dict(self.env,**updates),text=True,capture_output=True)
    def test_accepted_then_stapled_and_assessed(self):
        p=self.helper();self.assertEqual(p.returncode,0,p.stderr)
        calls=self.log.read_text()
        self.assertLess(calls.index('notarytool submit'),calls.index('stapler staple'))
        self.assertLess(calls.index('stapler validate'),calls.index('spctl --assess'))
        self.assertTrue(list((self.root/'out').glob('notary.*/result.json')))
    def test_rejected_never_staples(self):
        for status in ['Invalid','In Progress','malformed']:
            with self.subTest(status=status):
                self.log.write_text('')
                self.assertNotEqual(self.helper(APPLE_STATUS=status).returncode,0)
                self.assertNotIn('stapler',self.log.read_text())
                self.assertNotIn('spctl',self.log.read_text())
    def test_staple_failure_never_assesses(self):
        self.assertNotEqual(self.helper(FAIL_STAPLE='1').returncode,0)
        self.assertNotIn('spctl',self.log.read_text())
    def test_default_stops_before_build_without_credentials(self):
        env=self.env.copy();env.pop('NOTARY_PROFILE',None)
        p=subprocess.run(['bash',str(ROOT/'script/package.sh')],env=env,text=True,capture_output=True)
        self.assertNotEqual(p.returncode,0)
        self.assertIn('SIGNING_IDENTITY',p.stderr)
        self.assertFalse(self.log.exists())
    def test_missing_notary_profile_stops_before_build(self):
        env=dict(self.env,SIGNING_IDENTITY='Developer ID Application: Test (TEST123456)');env.pop('NOTARY_PROFILE',None)
        p=subprocess.run(['bash',str(ROOT/'script/package.sh')],env=env,text=True,capture_output=True)
        self.assertNotEqual(p.returncode,0)
        self.assertIn('NOTARY_PROFILE',p.stderr)
        self.assertFalse(self.log.exists())
    def test_ad_hoc_identity_rejected(self):
        p=subprocess.run(['bash',str(ROOT/'script/package.sh')],env=dict(self.env,SIGNING_IDENTITY='-'),text=True,capture_output=True)
        self.assertNotEqual(p.returncode,0)
        self.assertIn('Developer ID Application',p.stderr)
        self.assertFalse(self.log.exists())
    def test_unsigned_is_explicit_and_ignores_ambient_credentials(self):
        fixture = self.root / 'fixture'
        (fixture / 'script').mkdir(parents=True)
        (fixture / 'script/package.sh').write_text((ROOT / 'script/package.sh').read_text())
        (fixture / 'script/sync_version.py').write_text('')
        stub = fixture / 'script/build_core.sh'
        stub.write_text('#!/bin/sh\nexit 9\n');stub.chmod(0o755)
        xcode = self.bin / 'xcodebuild'
        xcode.write_text('#!/bin/sh\nexit 9\n');xcode.chmod(0o755)
        p=subprocess.run(['bash',str(fixture/'script/package.sh'),'--unsigned'],env=dict(self.env,SIGNING_IDENTITY='not a real identity'),text=True,capture_output=True)
        self.assertEqual(p.returncode,9,p.stderr)
        self.assertFalse(self.log.exists())
    def test_production_rejects_unsigned(self):
        args=['bash',str(ROOT/'script/package.sh'),'--unsigned']
        if ROOT.name == 'yacht': args.append('--production')
        p=subprocess.run(args,env=dict(self.env,EWAF_PRODUCTION='1'),text=True,capture_output=True)
        self.assertEqual(p.returncode,2,p.stderr)
        self.assertIn('requires notarization',p.stderr)
        self.assertFalse(self.log.exists())
    def test_profile_failure_stops_before_build(self):
        p=subprocess.run(['bash',str(ROOT/'script/package.sh')],env=dict(self.env,SIGNING_IDENTITY='Developer ID Application: Test (TEST123456)',FAIL_HISTORY='1'),text=True,capture_output=True)
        self.assertEqual(p.returncode,17,p.stderr)
        calls=self.log.read_text()
        self.assertIn('notarytool history',calls)
        self.assertNotIn('notarytool submit',calls)
    def test_final_zip_is_extracted_and_assessed(self):
        archive=self.root/'final.zip';archive.touch()
        p=subprocess.run(['bash',str(ROOT/'script/verify_notarized_zip.sh'),str(archive),'Example.app'],env=self.env,text=True,capture_output=True)
        self.assertEqual(p.returncode,0,p.stderr)
        calls=self.log.read_text()
        self.assertLess(calls.index('ditto -x -k'),calls.index('codesign --verify'))
        self.assertLess(calls.index('stapler validate'),calls.index('spctl --assess'))
        self.assertNotIn('notarytool submit',calls)
    def test_bad_final_signature_or_staple_fails(self):
        archive=self.root/'final.zip';archive.touch()
        for failure in ['FAIL_SIGNATURE','FAIL_STAPLE']:
            with self.subTest(failure=failure):
                self.log.write_text('')
                p=subprocess.run(['bash',str(ROOT/'script/verify_notarized_zip.sh'),str(archive),'Example.app'],env=dict(self.env,**{failure:'1'}),text=True,capture_output=True)
                self.assertNotEqual(p.returncode,0)
                self.assertNotIn('spctl',self.log.read_text())
    def test_development_output_labels_are_distinct(self):
        source=(ROOT/'script/package.sh').read_text()
        self.assertIn('PACKAGE_SUFFIX=-development-unnotarized',source)
        self.assertIn('$PACKAGE_ARCH$PACKAGE_SUFFIX',source)
        if ROOT.name == 'yacht':
            self.assertIn('PACKAGE_SUFFIX=-signed-unnotarized',source)
            self.assertIn('CLI_SUFFIX=-signed-unnotarized',source)
        else:
            self.assertIn('dist/notarized/EWAF.app',source)
            self.assertIn('APP_BUNDLE="$ROOT_DIR/dist/EWAF.app"',source)
    def test_final_archive_promoted_only_after_verification(self):
        source=(ROOT/'script/package.sh').read_text()
        self.assertLess(source.index('bash script/verify_notarized_zip.sh "$CANDIDATE"'), source.index('mv "$CANDIDATE"'))
    def test_unknown_option_rejected(self):
        p=subprocess.run(['bash',str(ROOT/'script/package.sh'),'--unknown'],env=self.env,text=True,capture_output=True)
        self.assertEqual(p.returncode,2)
        self.assertFalse(self.log.exists())

if __name__ == '__main__': unittest.main()
