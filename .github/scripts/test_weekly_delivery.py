import json
import unittest
from unittest.mock import patch, MagicMock
import weekly_delivery as d

ITEM = {'tag':'v1.0.7','sha':'a'*40,'target':'linux-x64'}
RELEASE = {'tag_name':'v1.0.7','draft':False,'assets':[{'name':'EWAF-1.0.7-macos-universal.zip','size':100,'state':'uploaded'}]}
BASE = {'schema':1,'repository':'tlolabs/ewaf','captured_at':'2026-11-13T00:00:00Z','entries':[]}

class DeliveryTests(unittest.TestCase):
    def test_requires_published_mac_package(self):
        self.assertTrue(d.mac_ready(RELEASE,ITEM['tag']))
        for release in ({},dict(RELEASE,draft=True),dict(RELEASE,assets=[]),dict(RELEASE,tag_name='v0.1.0')):
            self.assertFalse(d.mac_ready(release,ITEM['tag']))
    def test_preconditions_before_claim(self):
        api=MagicMock(repo='tlolabs/ewaf',root='repos/tlolabs/ewaf')
        api.pages.return_value=[]
        with patch.object(d.legacy.Gate,'select',return_value=({'include':[ITEM]},{})):
            matrix,summary=d.select(api,'v.*',['linux-x64'],BASE)
            self.assertEqual(matrix,{'include':[]})
            self.assertEqual(summary['deferred'],1)
        api.request.assert_not_called()
    def test_preserves_legacy_attempt_eligibility(self):
        api=MagicMock(repo='tlolabs/ewaf',root='repos/tlolabs/ewaf')
        api.pages.return_value=[dict(RELEASE,draft=True), RELEASE]
        with patch.object(d.legacy.Gate,'select',return_value=({'include':[ITEM]},{})) as select:
            self.assertEqual(d.select(api,'v.*',['linux-x64'],BASE)[0],{'include':[ITEM]})
            select.assert_called_once_with('v.*',['linux-x64'],baseline=BASE)
    def test_publish_not_smoke_controls_completion(self):
        api=MagicMock(repo='tlolabs/ewaf',root='repos/tlolabs/ewaf')
        for outcome in ('success','failure',None):
            api.pages.return_value=[{'name':'Smoke v1.0.7 (linux-x64)','status':'completed','conclusion':'success'},
                                    {'name':'Publish v1.0.7 (linux-x64)','status':'completed','conclusion':outcome}]
            with patch.object(d.legacy.Gate,'mark') as mark:
                self.assertEqual(d.finalize(api,{'include':[ITEM]},1,1,'url'),int(outcome!='success'))
                self.assertEqual(mark.call_args.args[1], 'success' if outcome=='success' else 'failure')
    def test_unstarted_work_keeps_recoverable_claim(self):
        api=MagicMock(repo='tlolabs/ewaf',root='repos/tlolabs/ewaf')
        api.pages.return_value=[]
        with patch.object(d.legacy.Gate,'mark') as mark:
            self.assertEqual(d.finalize(api,{'include':[ITEM]},1,1,'url'),1)
            mark.assert_not_called()
    def test_fillr_unsigned_restriction_preserved(self):
        api=MagicMock(repo='tlolabs/fillr',root='repos/tlolabs/fillr')
        self.assertFalse(d.ready_target(api,RELEASE,dict(ITEM,target='windows-x64')))
        api.request.assert_not_called()
    def test_fillr_windows_requires_packaging_support(self):
        import base64
        api=MagicMock(repo='tlolabs/fillr',root='repos/tlolabs/fillr')
        release=dict(RELEASE,prerelease=True,name='FILLR 1.0.7 unsigned downloads')
        for script,expected in [('param([switch]$SmokeOnly)',True),('old script',False)]:
            api.request.return_value={'encoding':'base64','content':base64.b64encode(script.encode()).decode()}
            self.assertEqual(d.ready_target(api,release,dict(ITEM,target='windows-x64')),expected)

if __name__=='__main__': unittest.main()
