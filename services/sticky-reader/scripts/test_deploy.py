import json,pathlib,subprocess,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parent.parent
class Deploy(unittest.TestCase):
 def test_public_without_auth_mount_or_env(self):
  s=json.loads((ROOT/'deploy/app.template.json').read_text())['items'][1]['spec']['template']['spec'];c=s['containers'][0]
  self.assertNotIn('SRP_VERIFIERS',[x['name'] for x in c['env']])
  self.assertEqual({x['name'] for x in s['volumes']},{'cache','tmp'})
  self.assertEqual({x['name'] for x in c['volumeMounts']},{'cache','tmp'})
 def test_scope_and_hard_limits(self):
  items=json.loads((ROOT/'deploy/app.template.json').read_text())['items']
  self.assertEqual([x['kind'] for x in items],['Namespace','Deployment','Service','Ingress','NetworkPolicy','NetworkPolicy'])
  for x in items:
   if x['kind']=='Namespace':self.assertEqual(x['metadata']['name'],'sticky-browser')
   else:self.assertEqual(x['metadata']['namespace'],'sticky-browser')
  d=items[1]['spec'];self.assertEqual(d['strategy']['type'],'Recreate');self.assertEqual(d['replicas'],1);s=d['template']['spec'];self.assertFalse(s['automountServiceAccountToken']);c=s['containers'][0]
  self.assertEqual(c['resources']['limits']['memory'],'192Mi');self.assertEqual(c['resources']['limits']['cpu'],'500m');self.assertTrue(c['securityContext']['readOnlyRootFilesystem']);self.assertEqual(c['securityContext']['capabilities']['drop'],['ALL']);self.assertEqual(c['imagePullPolicy'],'Never');self.assertTrue(s['securityContext']['runAsNonRoot']);self.assertEqual(s['securityContext']['seccompProfile']['type'],'RuntimeDefault')
  self.assertEqual(items[3]['spec']['rules'][0]['host'],'sticky.tobiasweissert.ch');self.assertEqual(items[3]['metadata']['annotations']['traefik.ingress.kubernetes.io/router.observability.accesslogs'],'false')
  expected={'sticky.tobiasweissert.ch','sticky-browser.tobiasweissert.ch'}
  self.assertEqual({x['host'] for x in items[3]['spec']['rules']},expected)
  self.assertEqual(set(items[3]['spec']['tls'][0]['hosts']),expected)
  out=subprocess.check_output([sys.executable,str(ROOT/'scripts/render_deploy.py'),'localhost/sticky-reader@sha256:'+'a'*64]);self.assertNotIn(b'IMAGE_DIGEST_REQUIRED',out)
if __name__=='__main__':unittest.main()
